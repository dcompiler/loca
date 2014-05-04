#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <map>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pin.H"
#include "portability.H"
#include "histo.H"
#include "rdtsc.H"
#include "atomic.H"
#include "instlib.H"
#include "sfp_list.H"

using namespace std;
using namespace histo;
using namespace INSTLIB;

/* ===================================================================== */
/* Global Macro definitions */
/* ===================================================================== */
#define MAX_PILLARS 8    // the max window length is no more than 2^34, 
                          // the lowest pillar is expected to be 2^12,
                          // therefore 2^34 / 2^12 = 2^22 = 4^11 ~= 8^6 pillars are needed
#define MAP_SIZE 0x7fffff // if long is 64bit, size should be larger 
#define MAX_THREAD 26     // max thread supported

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

/* knob of sharing graph dump file */
KNOB<string> KnobMemRefSampleFile(KNOB_MODE_WRITEONCE, "pintool",
			      "mr", "sample.out", "specify the memory reference sample file name");

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

/* metadata associated with each datum */
typedef TList<TStamp> TStampList;

/* simple bitmap type */
typedef uint64_t TBitset;

/* configures used in histo.H */
const  uint32_t              SUBLOG_BITS = 8;
const  uint32_t              MAX_WINDOW = (65-SUBLOG_BITS)*(1<<SUBLOG_BITS);

/* time stamp table entry, each entry is a set of time stamps */ 
typedef struct {
  PIN_MUTEX lock;
  map<ADDRINT, TStampList> set;
} TStampTblEntry;

typedef union {
  sfp_lock_t lock;
  char padding[WORDWIDTH];
} TPLock;

TPLock gWcountLock[MAX_THREAD];

/* global count of memory references */
#define MAX_SAMPLES 3000

TStamp gMemRefs[MAX_THREAD];
TStamp gSamples[MAX_SAMPLES][MAX_THREAD];

bool gEnableMemRefSampling = false;

#include "thread_support_privatized.H"

struct timeval start;         // start time of profiling
struct timeval finish;        // stop time of profiling

TStamp gStartTime = 0;
TStamp gEndTime = 0;

/* the output file stream */
ofstream ResultFile;
ofstream PerThreadResultFile;

/* the global wall time */
TStamp gWalltime;

volatile static TStamp N = 0; // trace length
volatile static TStamp N_sample = 0; // trace length
TPStamp M[MAX_THREAD];        // total memory footprint for each thread count
TStamp per_M[MAX_THREAD];        // total access count for each thread

INT64 wcount[MAX_THREAD][MAX_WINDOW];
INT64 wcount_i[MAX_THREAD][MAX_WINDOW];

INT64 per_wcount[MAX_THREAD][MAX_WINDOW];
INT64 per_wcount_i[MAX_THREAD][MAX_WINDOW];

/* global stamp table */
TStampTblEntry* gStampTbl;

/* the lowest pillar in log scale */
int gLowestPillar;

/* global pillar set */
TPLock gPillarsLock[MAX_PILLARS];

map<TBitset, TStamp> gPillars[MAX_PILLARS];

/* window lengths the pillars represent */
TStamp gPillarLengths[MAX_PILLARS];


/* ===================================================================== */
/* Routines */
/* ===================================================================== */


/* ========================================================
 * SFP Algorithm Logic
 * ======================================================== */
void SfpImpl(ADDRINT set_idx, ADDRINT addr, int tid, TStamp pos, local_stat_t* lstat) {

  TStampList s;

  /* find current address's stamp */
  s = gStampTbl[set_idx].set[addr];
 
  /* traverse the datum's access list to profile
   * the intervals
   * if current access is first access to the 
   * datum by this thread, following loop will
   * be skipped
   */
  TStampList::Iterator curr, prev=-1;
  int thd_count = 0;

  /* following loop profiles the pillar statistics, to obtain any thread set's fp */

  for(int i=0; i<MAX_PILLARS && pos>gPillarLengths[i]; i++)
  {
    TBitset bitmap = 0;
   
    /* high is the right most point a window's left end could reach*/
    TStamp high = pos - gPillarLengths[i];
    /* low is the left most point a window's left end could reach */
    TStamp low;
    if ( s.begin()!=-1 && s.get(s.begin()) > gPillarLengths[i] )
    {
      low = s.get(s.begin()) - gPillarLengths[i];
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
    
    for(curr=s.begin(); !s.is_end(curr); curr = s.next(curr))
    {
     /* by moving rpoint, we can determine the count of windows of
      * different sharer sets
      */
      TStamp c = s.get(curr);

      /* c > high means all these windows must contain thread 'iter' */
      if ( c > high )
      {
        bitmap |= ((TBitset)1<<curr);
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
      
      lstat->pillars[i][bitmap] += rpoint-c;

      /* update rpoint and bitmap */
      rpoint = c;
      bitmap |= ((TBitset)1<<curr);
    }

    lstat->pillars[i][bitmap] += rpoint-low;
  }

  /* profiling anyk-sfp statistics in following loop */

  for(thd_count=0, curr = s.begin(); !s.is_end(curr); curr=s.next(curr)) {

    /* if we reach the last access by tid */
    TStamp distance = pos - s.get(curr) - 1;

    /*
     * profile MI[thd_count][idx] and MI_i[thd_count][idx]
     */ 
    TStamp idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(distance);
    lstat->wcount[thd_count][idx]++;
    lstat->wcount_i[thd_count][idx] += distance;

    /* if tid is met, stop the traversal */
    if (curr == tid) {
      break;
    }

    /* always record the previous thread before tid */
    thd_count++;
    prev = curr;

    /*
     * profile SI[thd_count][idx] and SI_i[thd_count][idx]
     * which is equivalent to decreasing MI[thd_count][idx] and MI_i[thd_count][idx]
     */ 
    lstat->wcount[thd_count][idx]--;
    lstat->wcount_i[thd_count][idx] -= distance;
  }

  /* if iter is -1, the list is traversed without finding tid,
   * then this access is first access made by tid
   */
  if ( !s.is_end(curr) ) {

    if ( !s.is_end(prev) )
    {
      s.erase(prev, curr);
    }
  }
  else {

    /*
     * profile MI[thd_count][idx] and MI_i[thd_count][idx]
     */ 
    TStamp idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(pos-1);
    lstat->wcount[thd_count][idx]++;
    lstat->wcount_i[thd_count][idx] += pos-1;

    /* increment the corresponding the element in M,
     * because at each level of sharing, M should be different  
     */
    lstat->M[thd_count]++;
  }


  s.set_at(tid, pos);
  s.set_front(tid);
  
  /* set back addr's time stamp list */
  gStampTbl[set_idx].set[addr] = s;

}


/* ==================================================
 * Routine handling atomic trace processing
 * ================================================== */
VOID RecordMem(THREADID tid, VOID * ip, VOID * addr, UINT32 size, UINT32 type, ADDRINT sp)
{

  /* get tls handle before while loop to save some operations */
  local_stat_t* lstat = get_tls(tid);
  if( !lstat->enabled ) return;

  /* no profiling on stack variables */
  //if ( (ADDRINT)addr > sp )
  //{
  //  return;
  //}

  TStamp start = SFP_RDTSC();

  lstat->length++;
  

  /*
   * laddr is the lowest cacheline base touched by interval [addr, addr+size]
   * raddr is the highest cacheline base touched by interval [addr, addr+size]
   */
  ADDRINT laddr = (~WORDMASK)&((ADDRINT)addr);

  
  /* the index of set in gStampTbl */
  ADDRINT set_idx = (TStamp)SetIndex(laddr);

  /* reserve the locks for respective entries in gStampTbl before recording global time stamp */
  //lock_acquire(&gStampTbl[set_idx].lock);
  //if(!PIN_MutexTryLock(&gStampTbl[set_idx].lock)) return;
  PIN_MutexLock(&gStampTbl[set_idx].lock);

  /* atomic increment N, reserve next $size elements 
   * it has to be done after all $size elements are reserved
   */
  TStamp tempN = SFP_RDTSC() - gStartTime;
  
  SfpImpl(set_idx, laddr, tid, tempN, lstat );

  /* record access frequency */
  TStamp idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(tempN);
  lstat->per_wcount[idx]++;
  lstat->per_wcount_i[idx] += tempN; //sublog_index_to_value<MAX_WINDOW, SUBLOG_BITS>(idx);
  lstat->per_M++;

  /* release the locks on the entries associated with cur_addr */
  //lock_release(&gStampTbl[set_idx].lock);
  PIN_MutexUnlock(&gStampTbl[set_idx].lock);

  lstat->accum_time += SFP_RDTSC() - start;

}

/* ==================================================
 * Routine handling atomic trace processing
 * ================================================== */
VOID MemInstCount(THREADID tid)
{

  /* get tls handle before while loop to save some operations */
  local_stat_t* lstat = get_tls(tid);
  if( !lstat->enabled ) return;
 
  /* record all threads memory reference count */ 
  gMemRefs[tid]++;
  
  
}

  /* no profiling on stack variables */
/* =================================================
 * Routines for instrumentation controlling
 * ================================================= */

//
// activate instrumentation and recording
//
inline LOCALFUN VOID activate(THREADID tid) {

    /* enable sampling on global memory reference */
    if(!gEnableMemRefSampling) {
      gEnableMemRefSampling = true;
    }

    local_stat_t* data = get_tls(tid);
    data->enabled = true;
    __sync_bool_compare_and_swap(&gStartTime, 0, SFP_RDTSC());
}

//
// deactivate instrumentation and recording
//
inline LOCALFUN VOID deactivate(THREADID tid) {

    /* disable sampling on global memory reference */
    if(gEnableMemRefSampling) {
      gEnableMemRefSampling = false;
    }

    local_stat_t* data = get_tls(tid);
    data->enabled = false;
    gEndTime = SFP_RDTSC();
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
  cout << "pin id " << tid << ", system tid " << PIN_GetTid() << endl;
}

//
// hook at thread end
//
inline void ThreadFini_hook(THREADID tid, local_stat_t* lstat) {


  for(int i=0; i<MAX_THREAD; i++)
  {
    lock_acquire(&gWcountLock[i].lock);

    for(TStamp j=0; j<MAX_WINDOW; j++)
    {
      wcount[i][j]         += lstat->wcount[i][j];
      wcount_i[i][j]       += lstat->wcount_i[i][j];
    }

    M[i].con += lstat->M[i];
    lock_release(&gWcountLock[i].lock);

  }

  for(TStamp j=0; j<MAX_WINDOW; j++) {
    per_wcount[tid][j]   += lstat->per_wcount[j];
    per_wcount_i[tid][j] += lstat->per_wcount_i[j];
    per_M[tid] = lstat->per_M;
  }


  for(int i=0; i<MAX_PILLARS; i++)
  {
    lock_acquire(&gPillarsLock[i].lock);

    for(map<TBitset, TStamp>::iterator iter = lstat->pillars[i].begin(); 
        iter != lstat->pillars[i].end();
        iter++)
    {
      TBitset p = iter->first;
      TStamp  c = iter->second;
      gPillars[i][p] += c;
    }

    lock_release(&gPillarsLock[i].lock);
  }

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

    if ( memOperands > 0 ) {
      INS_InsertPredicatedCall(
             ins, IPOINT_BEFORE, (AFUNPTR)MemInstCount,
             IARG_THREAD_ID,
             IARG_END);
    }

    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {

      if (INS_MemoryOperandIsRead(ins, memOp)) {
         INS_InsertPredicatedCall(
             ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
             IARG_THREAD_ID,
             IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
             IARG_MEMORYREAD_SIZE,
             IARG_REG_VALUE, REG_STACK_PTR,
             IARG_END);
      }
      if (INS_MemoryOperandIsWritten(ins, memOp)) {
         INS_InsertPredicatedCall(
             ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
             IARG_THREAD_ID,
             IARG_INST_PTR, IARG_MEMORYOP_EA, memOp,
             IARG_MEMORYWRITE_SIZE,
             IARG_REG_VALUE, REG_STACK_PTR,
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
  
  int thd_count;
  TStampList::Iterator curr;
  INT64 temp[MAX_THREAD][MAX_WINDOW];
  INT64 temp_i[MAX_THREAD][MAX_WINDOW];

  /* complete the per-thread access frequency */
  for(int j=0;j<MAX_THREAD;j++) {
    for(TStamp i=0;i<MAX_WINDOW;i++) {
      temp[j][i] = 0;
    }

    for(TStamp i=0;i<MAX_WINDOW;i++) {
      if ( per_wcount[j][i] != 0) {
        INT64 total_i = per_wcount[j][i] * (gEndTime - gStartTime) - per_wcount_i[j][i];
        TStamp distance = total_i / per_wcount[j][i];
        int idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(distance);
        temp[j][idx] += per_wcount[j][i];
        temp_i[j][idx] += total_i;
      }
    }

    for(TStamp i=0;i<MAX_WINDOW;i++) {
      per_wcount[j][i] += temp[j][i];
      per_wcount_i[j][i] += temp_i[j][i];
    }
  }

  /* traversing all entries in gStampTbl */
  for(int i=0;i<MAP_SIZE+1;i++) {

    /* traversing all data in a set */
    for(map<ADDRINT, TStampList>::iterator entry = gStampTbl[i].set.begin(); entry!=gStampTbl[i].set.end(); entry++) {

      TStampList s = entry->second;

      /* the logic of profiling the leftover intervals is the same in SfpImpl */
      for(int k=0; k<MAX_PILLARS && N+1>gPillarLengths[k]; k++)
      {
        int bitmap = 0;
        TStamp high = N+1-gPillarLengths[k];
        TStamp low;

        if ( s.get(s.begin()) > gPillarLengths[k] )
        {
          low = s.get(s.begin()) - gPillarLengths[k];
        }
        else
        {
          low = 0;
        }

        TStamp rpoint = high;
        for(curr=s.begin(); !s.is_end(curr); curr=s.next(curr))
        {
          TStamp c = s.get(curr);
          if ( c <= low )  break;
          if ( c > high )
          {
            bitmap |= ((TBitset)1<<curr);
            continue;
          }
          gPillars[k][bitmap] += rpoint-c;
          rpoint = c;
          bitmap |= ((TBitset)1<<curr);
        }
        gPillars[k][bitmap] += rpoint - low;
      }

      /* traverse address's stamp's list to collect leftover intervals */
      for(curr=s.begin(), thd_count = 0; !s.is_end(curr); curr=s.next(curr), thd_count++) {

        TStamp distance = gEndTime - gStartTime - s.get(curr);
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
  ResultFile << dec << "N:" << N << " Threads: " << gThreadNum << " total_time:" << gWalltime  << endl;  
  ResultFile << "ws\t";
  for(TStamp j=0;j<MAX_THREAD;j++) {
    ResultFile << j+1 << "\t";
  }
  ResultFile << endl;

  stringstream ss;
  string filename = KnobResultFile.Value();
  ss.str(string());
  ss << filename << ".pt";
  PerThreadResultFile.open(ss.str().c_str());
  for(TStamp j=0;j<MAX_THREAD;j++) {
    PerThreadResultFile << per_M[j] << "\t";
  }
  PerThreadResultFile << endl;
  PerThreadResultFile << "ws\t";
  for(TStamp j=0;j<MAX_THREAD;j++) {
    PerThreadResultFile << j+1 << "\t";
  }
  PerThreadResultFile << endl;

}
  
//
// Routine for dumping the sharing graph
//
LOCALFUN VOID BuildSharingGraph()
{

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
    for( map<TBitset, TStamp>::iterator iter = gPillars[j].begin(); 
         iter != gPillars[j].end(); 
         iter++)
    {
      TBitset i = iter->first;
      char buffer[MAX_THREAD+1];
      
      int k = 0;
      // FIXME cast from TBitset to int, is it ok?
      TBitset n = i;
      do {
        buffer[k++] = n%2 + '0';
      } while ((n/=2)>0);
      buffer[k] = '\0';
      sharing_graph_file << buffer << '\t' << 1.0 * gPillars[j][i] / (N - gPillarLengths[j] + 1) << endl;
    }
    
    /* close file */
    sharing_graph_file.close();
  }
}

// 
// Print the memory reference samples into file
//
LOCALFUN VOID PrintMemRefSamples( ) {

  ofstream SampleFile;

  /* dump gSamples to file */
  SampleFile.open(KnobMemRefSampleFile.Value().c_str());
  
  for(int i = 0; i < MAX_SAMPLES; ++i ) {
    for(int j = 0; j < MAX_THREAD; ++j ) {
      SampleFile << gSamples[i][j] << "\t";
    }
    SampleFile << endl;
  }
  SampleFile << endl;

  SampleFile.close();
  
}

//
// Fini routine, called at application exit
//
VOID Fini(INT32 code, VOID* v) {
  
  /* get the time when finish */
  gettimeofday(&finish, 0);
  gWalltime = (finish.tv_sec - start.tv_sec);

  /* the sfp statistics */
  double sfp[MAX_THREAD];
  double per_freq[MAX_THREAD];
  
  /* buffer used to hold the sum of wcount and wcount_i arrays */
  double wcount_sum[MAX_THREAD], wcount_sum_i[MAX_THREAD];
  double per_wcount_sum[MAX_THREAD], per_wcount_sum_i[MAX_THREAD];

  TStamp j, ws;
  int i;
  N = gEndTime - gStartTime;  

  /* before analysis, collect the intervals left over at trace end */
  CollectLastAccesses();

  /* dump the sharing graph from gPillars profile */
  BuildSharingGraph();

  /* dump the memory reference sample file */
  PrintMemRefSamples();

  /* clean up the allocated thread local data */
  ThreadEnd();

  /* open up output files and dump the statistics in */
  OpenOutputFile();

  /* initialize the sfp array to 0 and wcount_sum(_i) arrays to the sum of corresponding arrays */
  for (i=0;i<MAX_THREAD;i++) {

    wcount_sum[i] = 0;
    wcount_sum_i[i] = 0;
    per_wcount_sum[i] = 0;
    per_wcount_sum_i[i] = 0;
    sfp[i] = 0;
    per_freq[i] = 0;

  }

  for(i=0;i<MAX_THREAD;i++) {
    for(j=1;j<=sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(N+1);j++){

      wcount_sum[i] += wcount[i][j];
      wcount_sum_i[i] += wcount_i[i][j];
      per_wcount_sum[i] += per_wcount[i][j];
      per_wcount_sum_i[i] += per_wcount_i[i][j];
      
    }
  }

  /* actual compute comes here */

  for(j=1;j<=sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(N);j++){

    ws = sublog_index_to_value<MAX_WINDOW, SUBLOG_BITS>(j);

    /* first column is the window length */
    ResultFile << ws;
    PerThreadResultFile << ws;

    for(i=0;i<MAX_THREAD;i++) {

      sfp[i] = 1.0 * (wcount_sum_i[i] - (ws-1)*wcount_sum[i]) / (N-ws+1);    
      sfp[i] = M[i].con - sfp[i];

      wcount_sum[i] -= wcount[i][j];
      wcount_sum_i[i] -= wcount_i[i][j];

      /* one column for each sharing degree */
      ResultFile << "\t" << setprecision(12) << sfp[i]*WORDWIDTH;

      per_freq[i] = 1.0 * (per_wcount_sum_i[i] - (ws-1)*per_wcount_sum[i]) / (N-ws+1);    
      per_freq[i] = per_M[i] - per_freq[i];

      per_wcount_sum[i] -= per_wcount[i][j];
      per_wcount_sum_i[i] -= per_wcount_i[i][j];

      /* one column for each sharing degree */
      PerThreadResultFile << "\t" << setprecision(12) << per_freq[i];
      
    }

    ResultFile << endl;
    PerThreadResultFile << endl;
  }

  ResultFile.close();  
  PerThreadResultFile.close();  

  for(TStamp i=0; i<(MAP_SIZE+1); i++)
    PIN_MutexFini(&gStampTbl[i].lock);
  /* deallocate the global stamp table */
  delete[] gStampTbl;
}

//
// The replacing routine for SFP_TaskStart
//
void BeforeTaskStart(int tid) {
  
  //cout << "BeforeTaskStart " << tid << endl;
  local_stat_t* lstat = get_tls(PIN_ThreadId());
  if ( tid > MAX_THREAD )
  {
    cerr << "task id is too large" << endl;
    return;
  }
  lstat->tasks.push_back(tid);
  lstat->current_task = tid;

}

//
// The replacing routine for SFP_TaskEnd
//
void BeforeTaskEnd(int tid) {

  //cout << "BeforeTaskEnd " << tid << endl;
  local_stat_t* lstat = get_tls(PIN_ThreadId());
  if ( tid != lstat->current_task || tid != lstat->tasks.back() )
  {
    cerr << "task ends at wrong task id" << endl;
    return;
  }
  lstat->tasks.pop_back();
  lstat->current_task = lstat->tasks.back();

}

//
// Replacing SFP_TaskStart and SFP_TaskEnd to
// get the user provided task id
//
// These two routines must NOT be inlined
//
VOID ImageLoad( IMG img, VOID* v) {

  if ( IMG_IsMainExecutable(img) )
  {
    RTN start_rtn = RTN_FindByName( img, "SFP_TaskStart" );
    RTN end_rtn = RTN_FindByName( img, "SFP_TaskEnd" );

    if (RTN_Valid(start_rtn) && RTN_Valid(end_rtn))
    {
      cout << "replaced" << endl;

      RTN_Replace(start_rtn, AFUNPTR(BeforeTaskStart));
      RTN_Replace(end_rtn,   AFUNPTR(BeforeTaskEnd));
    }

  }
}

VOID MemRefCollector(VOID* arg) {

  int index = 0;

  while(1) {

    if ( gEnableMemRefSampling ) {

      for(int i = 0; i < MAX_THREAD; ++i ) {
        gSamples[index%MAX_SAMPLES][i] = gMemRefs[i];
      }

      index++;

    }
    // milliseconds
    // 1 second = 1000 milliseconds
    PIN_Sleep(500);

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
      PIN_MutexInit(&gStampTbl[i].lock);

    for(int i=0; i<MAX_THREAD; i++)
    {
      for(TStamp j=0; j<MAX_WINDOW; j++)
      {
        wcount[i][j] = wcount_i[i][j] = 0;
      }
    }
 
    for(int i=0; i<MAX_THREAD; i++)
    {
      gMemRefs[i] = 0;
    }

    /* allocate space for gPillars and setup pillar lengths */
    gLowestPillar = KnobLPillar.Value();
    //gPillarLengths[0] = 1 << gLowestPillar;
    for(int i=0; i<MAX_PILLARS; i++)
    {
      gPillarLengths[i] = ((TStamp)1)<<(2*i+gLowestPillar);

      lock_release(&gPillarsLock[i].lock);
    }
      
    /* check for knobs if region instrumentation is involved */
    control.RegisterHandler(ControlHandler, 0, FALSE);
    control.Activate();

    /* register callbacks */
    TRACE_AddInstrumentFunction(Trace, 0);
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    PIN_AddFiniFunction(Fini, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);

    PIN_SpawnInternalThread(MemRefCollector, 
                            NULL, /* arg */
                            0,    /* default stack size */
                            NULL);

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

