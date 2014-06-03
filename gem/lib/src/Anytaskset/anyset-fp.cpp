#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <map>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>

#include "pin.H"
#include "portability.H"
#include "histo.H"
#include "atomic.h"
#include "instlib.H"

using namespace std;
using namespace histo;
using namespace INSTLIB;

/* ===================================================================== */
/* Global Macro definitions */
/* ===================================================================== */
#define MAX_PILLARS 12    // the max window length is no more than 2^34, 
                          // the lowest pillar is expected to be 2^12,
                          // therefore 2^34 / 2^12 = 2^22 = 4^11 pillars are needed
#define MAP_SIZE 0x7fffff // if long is 64bit, size should be larger 
#define MAX_THREAD 22     // max thread supported

#define SETSHIFT 6
#define WORDSHIFT 6

#define SETWIDTH (1<<SETSHIFT)    // a lock is of granularity 64 bytes, a cacheblock size
#define WORDWIDTH (1<<WORDSHIFT)  // a word is of granularity 64 bytes

#define SETMASK (SETWIDTH-1)      // a mask of set, equivalently, used in getting the lower (SETSHIFT-1) bits
#define WORDMASK (WORDWIDTH-1)    // a mask of word, equivalently, used in getting the lower (WORDSHIFT-1) bits

#define SetIndex(x) (((x)>>SETSHIFT)&MAP_SIZE)
#define WordIndex(x) ((x)&~WORDMASK)

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */

/* knob of output file */
KNOB<string> KnobResultFile(KNOB_MODE_WRITEONCE, "pintool",
			    "o", "fp.out", "specify result file name");

/* A Pillar is a particular window length, for which, our tools accurately 
 * measure any thread set's shared footprint
 */
/* knob of lowest pillar */
KNOB<int> KnobLPillar(KNOB_MODE_WRITEONCE, "pintool",
			 "l", "12", "specify the lowest pillar in log scale");

/* knob of sharing graph dump file */
KNOB<string> KnobSharingGraphFile(KNOB_MODE_WRITEONCE, "pintool",
			      "g", "sg.out", "specify the sharing graph file name");


/* control variable */
LOCALVAR CONTROL control;

/* time stamp type */
typedef UINT64 TStamp;

/* cacheline padded time stamp type */
typedef union {
  TStamp con;
  char padding[WORDWIDTH];
} TPStamp;

/* a compact list of time stamps */
typedef struct {
  TStamp latest;
  char next;
} TStampListElement;

/* metadata associated with each datum */
typedef struct TStampList_t {

  /* a compact list of time stamps */
  TStampListElement list[MAX_THREAD];
  char head;

  TStampList_t() : head(-1) {
    for(int i=0;i<MAX_THREAD;i++) {
      list[i].latest = 0;
      list[i].next = -1;
    }
  }
} TStampList;

/* configures used in histo.H */
const  uint32_t              SUBLOG_BITS = 8;
const  uint32_t              MAX_WINDOW = (65-SUBLOG_BITS)*(1<<SUBLOG_BITS);

/* time stamp table entry, each entry is a set of time stamps */ 
typedef struct {
  sfp_lock_t lock;
  map<ADDRINT, TStampList> set;
} TStampTblEntry;

#include "thread_support.H"

struct timeval start;         // start time of profiling
struct timeval finish;        // stop time of profiling

/* the output file stream */
ofstream ResultFile;

/* the global wall time */
TStamp gWalltime;

volatile static TStamp N = 0; // trace length
TPStamp M[MAX_THREAD];        // total memory footprint for each thread count

INT64 wcount[MAX_THREAD][MAX_WINDOW];
INT64 wcount_i[MAX_THREAD][MAX_WINDOW];

/* global stamp table */
TStampTblEntry* gStampTbl;

/* the lowest pillar in log scale */
int gLowestPillar;

/* global pillar set */
TStamp* gPillars[MAX_PILLARS];

/* window lengths the pillars represent */
TStamp gPillarLengths[MAX_PILLARS];

/* ===================================================================== */
/* Routines */
/* ===================================================================== */


/* ========================================================
 * SFP Algorithm Logic
 * ======================================================== */
void SfpImpl(ADDRINT set_idx, ADDRINT addr, int tid, TStamp pos) {

  TStampList s;

  /* find current address's stamp */
  s = gStampTbl[set_idx].set[addr];
 
  int head = s.head;
  
  /* traverse the datum's access list to profile
   * the intervals
   * if current access is first access to the 
   * datum by this thread, following loop will
   * be skipped
   */
  int iter;
  int thd_count = 0;
  int prev = -1;

  /* following loop profiles the pillar statistics, to obtain any thread set's fp */
  for(int i=0; i<MAX_PILLARS && pos>gPillarLengths[i]; i++)
  {
    // FIXME use int to store a bitmap, is it OK?
    int bitmap = 0;
   
    /* high is the right most point a window's left end could reach*/
    TStamp high = pos - gPillarLengths[i];
    /* low is the left most point a window's left end could reach */
    TStamp low;
    if ( head != -1 && s.list[head].latest > gPillarLengths[i] )
    {
      low = s.list[head].latest - gPillarLengths[i];
    }
    else
    {
      low = 0;
    }

    /* rpoint is a cursor, which represent a window's right end.
     * rpoint is the dividing point where a window that right-ends
     * at right side of rpoint has different sharer patterns 
     * than the ones that right-ends at left side of rpoint.
     * rpoint corresponds to the time stamps appeared in current
     * datum's time stamp list
     */
    TStamp rpoint = high;
    for(iter=head; iter!=-1; iter=s.list[iter].next)
    {
     /* by moving rpoint, we can determine the count of windows of
      * different sharer sets
      */
      TStamp c = s.list[iter].latest;

      /* c > high means all these windows must contain thread 'iter' */
      if ( c > high )
      {
        bitmap |= (1<<iter);
        continue;
      }

      /* c <= low means all these windows can not contain following threads 
       * profiling for pillars is over
       */
      if ( c <= low )
      {
        break;
      }
      
      /* if c is in [low, high], profile rpoint-c, that is the count
       * of windows with sharer pattern bitmap 
       */
      __sync_fetch_and_add(&gPillars[i][bitmap], rpoint-c);

      /* update rpoint and bitmap */
      rpoint = c;
      bitmap |= (1<<iter);
    }

    __sync_fetch_and_add(&gPillars[i][bitmap], rpoint-low);
  }

  for(iter = head; iter != -1; iter = s.list[iter].next) {

    /* if we reach the last access by tid */
    TStamp distance = pos - s.list[iter].latest - 1;

    /*
     * profile MI[thd_count][idx] and MI_i[thd_count][idx]
     */ 
    TStamp idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(distance);
    __sync_add_and_fetch(&wcount[thd_count][idx], 1);
    __sync_add_and_fetch(&wcount_i[thd_count][idx], distance);

    /* if tid is met, stop the traversal */
    if (iter == tid) {
      break;
    }

    /* always record the previous thread before tid */
    prev = iter;
    thd_count++;

    /*
     * profile SI[thd_count][idx] and SI_i[thd_count][idx]
     * which is equivalent to decreasing MI[thd_count][idx] and MI_i[thd_count][idx]
     */ 
    __sync_sub_and_fetch(&wcount[thd_count][idx], 1);
    __sync_sub_and_fetch(&wcount_i[thd_count][idx], distance);

  }

  /* if iter is -1, the list is traversed without finding tid,
   * then this access is first access made by tid
   */
  if ( iter == -1 ) {

    TStamp idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(pos-1);

    /*
     * profile MI[thd_count][idx] and MI_i[thd_count][idx]
     */ 
    __sync_add_and_fetch(&wcount[thd_count][idx], 1);
    __sync_add_and_fetch(&wcount_i[thd_count][idx] , pos-1);

    /* increment the corresponding the element in M,
     * because at each level of sharing, M should be different  
     */
    __sync_add_and_fetch(&M[thd_count].con, 1);
  }
   
  /* update the latest access time of tid to pos */
  s.list[tid].latest = pos;
  
  /* prev is not -1, prev *MUST* points to tid's previous thread */
  if ( prev != -1 ) {
    s.list[prev].next = s.list[tid].next;
    s.list[tid].next = s.head;
  }

  /* update head of the stamp list */
  s.head = tid;

  /* set back addr's time stamp list */
  gStampTbl[set_idx].set[addr] = s;

}


/* ==================================================
 * Routine handling atomic trace processing
 * ================================================== */
VOID RecordMem(THREADID tid, VOID * ip, VOID * addr, UINT32 size, UINT32 type)
{

  /* get tls handle before while loop to save some operations */
  local_stat_t* lstat = get_tls(tid);
  if( !lstat->enabled ) return;

  /*
   * laddr is the lowest cacheline base touched by interval [addr, addr+size]
   * raddr is the highest cacheline base touched by interval [addr, addr+size]
   */
  ADDRINT laddr = (~WORDMASK)&((ADDRINT)addr);
  ADDRINT raddr = laddr + size; 

  /* the index of set in gStampTbl */
  ADDRINT set_idx;

  /* reserve the locks for respective entries in gStampTbl before recording global time stamp */
  for( ADDRINT base_addr = laddr; base_addr < raddr; base_addr += SETWIDTH) {
    lock_acquire(&gStampTbl[SetIndex(base_addr)].lock);
  }

  /* atomic increment N, reserve next $size elements 
   * it has to be done after all $size elements are reserved
   */
  TStamp tempN = __sync_add_and_fetch(&N, 1);
  
  for( ADDRINT cur_addr = laddr; cur_addr < raddr; cur_addr += SETWIDTH) {

    set_idx = (TStamp)SetIndex(cur_addr);

    if( cur_addr < raddr ) 
    {
      SfpImpl(set_idx, cur_addr, tid, tempN);
    }

    /* release the locks on the entries associated with cur_addr */
    lock_release(&gStampTbl[set_idx].lock);

  }
}

/* =================================================
 * Routines for instrumentation controlling
 * ================================================= */

//
// activate instrumentation and recording
//
inline LOCALFUN VOID activate(THREADID tid) {
    local_stat_t* data = get_tls(tid);
    data->enabled = true;
}

//
// deactivate instrumentation and recording
//
inline LOCALFUN VOID deactivate(THREADID tid) {
    local_stat_t* data = get_tls(tid);
    data->enabled = false;
}

//
// hook at thread launch
//
inline void ThreadStart_hook(THREADID tid, local_stat_t* tdata) {
  // FIXME: the controller starts all threads if no trigger conditions
  // are specified, but currently it only starts TID0. Starting here
  // is wrong if the controller has a nontrivial start condition, but
  // this is what most people want. They can always stop the controller
  // and using markers as a workaround
  if(tid) {
    activate(tid);
  }
}

//
// hook at thread end
//
inline void ThreadFini_hook(THREADID tid, local_stat_t* tdata) {

}

//
// callbacks of control handler
//
LOCALFUN VOID ControlHandler(CONTROL_EVENT ev, VOID* val, CONTEXT *ctxt, VOID* ip, THREADID tid) {
  switch(ev) {
    case CONTROL_START: // start
      activate(tid);
      break;
    case CONTROL_STOP:  // stop
      deactivate(tid);
      break;
    default:
      ASSERTX(false);
  }
}

/* ==================================================
 * Rountines for instrumentation
 * ================================================== */

//
// instruction callback
//
VOID Instruction(INS ins, VOID *v) {
    // the instrumentation is called iff the instruction will actually be executed.
    // The IA-64 architecture has explicitly predicated instructions.
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP
    // prefixed instructions appear as predicated instructions in Pin
    //
    
    UINT32 memOperands = INS_MemoryOperandCount(ins);
    
    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {

      if (INS_MemoryOperandIsRead(ins, memOp)) {
         INS_InsertPredicatedCall(
             ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
             IARG_THREAD_ID,
             IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
             IARG_MEMORYREAD_SIZE,
             IARG_END);
      }
      if (INS_MemoryOperandIsWritten(ins, memOp)) {
         INS_InsertPredicatedCall(
             ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
             IARG_THREAD_ID,
             IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
             IARG_MEMORYWRITE_SIZE,
             IARG_END);
      }
    }
}

//
// trace callback
//
VOID Trace(TRACE trace, VOID* v) {

   // head of trace
   ADDRINT pc = TRACE_Address(trace);

   for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) {
     
      // head of basic block
      const INS head = BBL_InsHead(bbl);

      if (!INS_Valid(head)) continue;
      
      for (INS ins = head; INS_Valid(ins); ins = INS_Next(ins)) {

          unsigned int instruction_size = INS_Size(ins);
          Instruction(ins, v);

          pc += instruction_size;
      }
   }
}

//
// helper routine in Fini
// to collect the intervals with right end at the end of trace
//
LOCALFUN VOID CollectLastAccesses() {
  
  int j, thd_count;

  /* traversing all entries in gStampTbl */
  for(int i=0;i<MAP_SIZE+1;i++) {

    /* traversing all data in a set */
    for(map<ADDRINT, TStampList>::iterator iter = gStampTbl[i].set.begin(); iter!=gStampTbl[i].set.end(); iter++) {

      TStampList s = iter->second;
      int head = s.head;
      
      /* the logic of profiling the leftover intervals is the same in SfpImpl */
      for(int k=0; k<MAX_PILLARS && N+1>gPillarLengths[k]; k++)
      {
        int bitmap = 0;
        TStamp high = N+1-gPillarLengths[k];
        TStamp low;

        if ( s.list[head].latest > gPillarLengths[k] )
        {
          low = s.list[head].latest - gPillarLengths[k];
        }
        else
        {
          low = 0;
        }

        TStamp rpoint = high;
        for(j=s.head; j!=-1; j=s.list[j].next)
        {
          TStamp c = s.list[j].latest;
          if ( c <= low )  break;
          if ( c > high )
          {
            bitmap |= (1<<j);
            continue;
          }
          gPillars[k][bitmap] += rpoint-c;
          rpoint = c;
          bitmap |= (1<<j);
        }
        gPillars[k][bitmap] += rpoint - low;
      }
 
      /* traverse address's stamp's list to collect leftover intervals */
      for(j=s.head, thd_count = 0; j!=-1; j=s.list[j].next, thd_count++) {

        TStamp distance = N - s.list[j].latest;
        TStamp idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(distance);

        /*
         * increment MI[thd_count][idx] and MI_i[thd_count][idx]
         */ 
        wcount[thd_count][idx]++;
        wcount_i[thd_count][idx] += distance;

      }
    }
  }
}

//
// routine for openning output file
//
LOCALFUN VOID OpenOutputFile() {

  ResultFile.open(KnobResultFile.Value().c_str());
  ResultFile << dec << "N:" << N << " Memory size: " << MAP_SIZE << " total_time:" << gWalltime  << endl;  
  ResultFile << "ws\t";
  for(TStamp j=0;j<MAX_THREAD;j++) {
    ResultFile << j+1 << "\t";
  }
  ResultFile << endl;

}
  
//
// Routine for dumping the sharing graph
//
LOCALFUN VOID BuildSharingGraph()
{
  // FIXME is it ok here to use int to represent a bitmap
  int max_index = 1<<gThreadNum;

  /* dump gPillars profile to file */
  ofstream sharing_graph_file;
  stringstream ss;
  string filename = KnobSharingGraphFile.Value();

  for( int j=0; j<MAX_PILLARS; j++)
  {
    /* don't need to dump pillars larger than N */
    if ( gPillarLengths[j] > N ) break;

    /* open files */
    ss.str(string());
    ss << filename << "." << (j+gLowestPillar);
    sharing_graph_file.open(ss.str().c_str());

    /* dump to files */
    for( int i=1; i<max_index; i++)
    {
      char buffer[MAX_THREAD+1];
      int n = i;
      int k = 0;
      do {
        buffer[k++] = n%2 + '0';
      } while ((n/=2)>0);
      buffer[k] = '\0';
      
      /* only dump the non zero thread set's pillars */
      if ( gPillars[j][i] != 0 )
      {
        sharing_graph_file << buffer << '\t' << 1.0 * gPillars[j][i] / (N - gPillarLengths[j] + 1) << endl;
      }
    }
    
    /* close file */
    sharing_graph_file.close();
  }
}


//
// Fini routine, called at application exit
//
VOID Fini(INT32 code, VOID* v){
  
  /* get the time when finish */
  gettimeofday(&finish, 0);
  gWalltime = (finish.tv_sec - start.tv_sec);

  /* the sfp statistics */
  double sfp[MAX_THREAD];
  
  /* buffer used to hold the sum of wcount and wcount_i arrays */
  double wcount_sum[MAX_THREAD], wcount_sum_i[MAX_THREAD];

  TStamp j, ws;
  int i;
  
  /* before analysis, collect the intervals left over at trace end */
  CollectLastAccesses();

  /* dump the sharing graph from gPillars profile */
  BuildSharingGraph();

  /* clean up the allocated thread local data */
  ThreadEnd();

  /* open up output files and dump the statistics in */
  OpenOutputFile();

  /* initialize the sfp array to 0 and wcount_sum(_i) arrays to the sum of corresponding arrays */
  for (i=0;i<MAX_THREAD;i++) {

    wcount_sum[i] = 0;
    wcount_sum_i[i] = 0;
    sfp[i] = 0;

  }

  for(i=0;i<MAX_THREAD;i++) {
    for(j=1;j<=sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(N+1);j++){

      wcount_sum[i] += wcount[i][j];
      wcount_sum_i[i] += wcount_i[i][j];
      
    }
  }

  /* actual compute comes here */

  for(j=1;j<=sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(N);j++){

    ws = sublog_index_to_value<MAX_WINDOW, SUBLOG_BITS>(j);

    /* first column is the window length */
    ResultFile << ws;

    for(i=0;i<MAX_THREAD;i++) {

      sfp[i] = 1.0 * (wcount_sum_i[i] - (ws-1)*wcount_sum[i]) / (N-ws+1);    
      sfp[i] = M[i].con - sfp[i];

      wcount_sum[i] -= wcount[i][j];
      wcount_sum_i[i] -= wcount_i[i][j];

      /* one column for each sharing degree */
      ResultFile << "\t" << setprecision(12) << sfp[i]*WORDWIDTH;
      
    }

    ResultFile << endl;
  }

  ResultFile.close();  

  /* deallocate the global stamp table */
  delete[] gStampTbl;
  for(int i=0; i<MAX_PILLARS; i++)
  {
    delete[] gPillars[i];
  }
}

/* =====================================================
 * main routine, entry of pin tools
 * ===================================================== */

//
// helper to output error message
//
LOCALFUN INT32 Usage() {
    cerr << "This tool prints out the number of dynamic instructions executed to stderr.\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

//
// main
//
int main(int argc, char *argv[])
{
    PIN_InitSymbols();
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    /* allocate space for gStampTbl */
    gStampTbl = new TStampTblEntry[MAP_SIZE+1];
    for(TStamp i=0; i<(MAP_SIZE+1); i++)
      lock_release(&gStampTbl[i].lock);
 
    /* allocate space for gPillars and setup pillar lengths */
    gLowestPillar = KnobLPillar.Value();
    gPillarLengths[0] = 1 << gLowestPillar;
    for(int i=0; i<MAX_PILLARS; i++)
    {
      if ( i!=0 )
      {
        gPillarLengths[i] = gPillarLengths[i-1]*4;
      }
      gPillars[i] = new TStamp[1<<MAX_THREAD];
      for(int j=0; j<(1<<MAX_THREAD); j++)
      {
        gPillars[i][j] = 0;
      }
    }
      
    /* check for knobs if region instrumentation is involved */
    control.RegisterHandler(ControlHandler, 0, FALSE);
    control.Activate();

    /* register callbacks */
    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddFiniFunction(Fini, 0);

    /* init thread hooks, implemented in thread_support.H */
    ThreadInit();

    /* start timing now */
    gettimeofday(&start, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */

