#include <iostream>
#include <fstream>
#include <stdlib.h>

#include "pin.H"
#include "xed-portability.h"
#include "histo.H"

using namespace std;
using namespace histo;
KNOB<string> KnobResultFile(KNOB_MODE_WRITEONCE, "pintool",
			    "o", "fp.out", "specify result file name");

#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include "histo.H"
//#include <math.h>

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */
std::ofstream ResultFile;

#define MAP_SIZE 0x4000000 //todo: if long is 64bit, size should be larger 
//#define MAP_SIZE (1<<18) //around cache size: 8M = 2**23/(2**6) = 2**17 
#define WORDSHIFT 6
#define Index(x) (x>>WORDSHIFT)%MAP_SIZE
// #define MAX_WINDOW 225 //8*28+1 do not count the window size>1 billion
typedef unsigned int stamp_t;
typedef unsigned long stamp_addr;
typedef unsigned long long stamp_tt;

const  uint32_t              SUBLOG_BITS = 8;
const  uint32_t              MAX_WINDOW = (65-SUBLOG_BITS)*(1<<SUBLOG_BITS);
//const uint32_t MAX_WINDOW = 5890; //index_to_value(5888) = 1 billion

static stamp_tt N = 0, M = 0;//N: length of trace; M: maximum of footprint (total amount of different data); 
static stamp_tt stamps[MAP_SIZE];//Used as address hash
static stamp_tt first_count_i[MAX_WINDOW];
static stamp_tt first_my_count[MAX_WINDOW];
static stamp_tt count_i[MAX_WINDOW];
static stamp_tt my_count[MAX_WINDOW];
struct timeval start;//start time of profiling
struct timeval finish;//stop time of profiling

/* ===================================================================== */

INT32 Usage() {
    cerr << "This tool prints out the number of dynamic instructions executed to stderr.\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}

/* ===================================================================== */



/*
 * Callback Routines
 */
//#define threshold (1<<30)
//#define cache_threshold (1<<17)

VOID RecordMem(VOID * ip, VOID * addr)
{
  ADDRINT ref = (ADDRINT)addr;
  stamp_t idx = Index(ref);
  stamp_tt prev_access = stamps[idx];
  stamps[idx] = ++N;

  idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS> (N-prev_access);

  if(!prev_access){
    M++;
    first_count_i[idx] += N - prev_access;
    first_my_count[idx] += 1;
  }else{
    count_i[idx] += N - prev_access;
    my_count[idx] += 1;
  }

  //  if(( N >= threshold) || (M >= cache_threshold))
  //    {
  //    PIN_ExitApplication(0);
  //  }

}


VOID Instruction(INS ins, VOID *v) {
    // Instruments loads using a predicated call, i.e.
    // the call happens iff the load will be actually executed.
    // The IA-64 architecture has explicitly predicated instructions. 
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
    if (INS_IsMemoryRead(ins)) {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
			   IARG_INST_PTR, IARG_MEMORYREAD_EA, 
			   IARG_END);
    }
    if (INS_HasMemoryRead2(ins)) {
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
			   IARG_INST_PTR, IARG_MEMORYREAD2_EA, 
			   IARG_END);
    }
    if (INS_IsMemoryWrite(ins))	{
      INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordMem,
			   IARG_INST_PTR, IARG_MEMORYWRITE_EA, 
			   IARG_END);
    }
}



/* ===================================================================== */

VOID Fini(INT32 code, VOID *v){
  
  gettimeofday(&finish, 0);
  stamp_t time = (finish.tv_sec - start.tv_sec);
  stamp_t i, idx;
  stamp_tt ws, tmp;
  
  ResultFile.open(KnobResultFile.Value().c_str());
  ResultFile << "N:" << N << " M:" << M << " total_time:" << time  << endl;
  
  for(i=0;i<MAP_SIZE;i++){
    if(!stamps[i])
      continue;
    tmp = N-stamps[i]+1;
    idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(tmp);
    first_my_count[idx]+=1;
    first_count_i[idx]+=tmp;
  }

  double sum=0.0, sum_i=0.0, fp, mr=1.0;
  
  for(i=1;i<=sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(N);i++){
    ws = sublog_index_to_value<MAX_WINDOW, SUBLOG_BITS>(i);
    fp = (1.0*((N-sum)*ws+sum_i))/(N-ws+1);
    sum_i += count_i[i] + first_count_i[i];
    sum += my_count[i] + first_my_count[i];
    //    ResultFile << i << "\t" << ws << "\t" << fp << "\t" << count_i[i] << "\t";
    //ResultFile << my_count[i] << "\t" << first_count_i[i] << "\t" << first_my_count[i] << endl;
    mr -= my_count[i]*1.0/N;
    ResultFile << i << "\t" << ws << "\t" << fp << "\t" <<  my_count[i]*1.0/N << "\t" << mr << endl;
  }
  ResultFile.close();  
  
}

/* ===================================================================== */

int main(int argc, char *argv[])
{
    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    memset (stamps, 0, MAP_SIZE*sizeof(stamp_t));
    memset (my_count, 0, MAX_WINDOW*sizeof(stamp_tt));
    memset (count_i, 0, MAX_WINDOW*sizeof(stamp_tt));

    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    gettimeofday(&start, 0);

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
