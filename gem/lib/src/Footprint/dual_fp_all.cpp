#include <iostream>
#include <fstream>
#include <stdlib.h>

#include "pin.H"
#include "xed-portability.h"
#include "histo.H"

using namespace std;
using namespace histo;
KNOB<string> KnobResultFile(KNOB_MODE_WRITEONCE, "pintool",
			    "o", "dual_fp.out", "specify result file name");
KNOB<UINT32> KnobProfileMode(KNOB_MODE_WRITEONCE, "pintool", 
			    "m","1", "instruction or/and data: data(1), inst(2), both(3)");    

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
#define WORDSHIFT1 6
#define WORDSHIFT2 2
#define Index1(x) (x>>WORDSHIFT1)%MAP_SIZE
#define Index2(x) (x>>WORDSHIFT2)%MAP_SIZE
// #define MAX_WINDOW 225 //8*28+1 do not count the window size>1 billion
typedef unsigned int stamp_t;
typedef unsigned long stamp_addr;
typedef unsigned long long stamp_tt;

const  uint32_t              SUBLOG_BITS = 8;
const  uint32_t              MAX_WINDOW = (65-SUBLOG_BITS)*(1<<SUBLOG_BITS);
//const uint32_t MAX_WINDOW = 5890; //index_to_value(5888) = 1 billion

static stamp_tt N = 0;//N: length of trace; M: maximum of footprint (total amount of different data); 
static stamp_tt M[2], stamps[MAP_SIZE][2];//Used as address hash
static stamp_tt first_count_i[MAX_WINDOW][2];
static stamp_tt first_count[MAX_WINDOW][2];
static stamp_tt count_i[MAX_WINDOW][2];
static stamp_tt lcount[MAX_WINDOW][2];
struct timeval start;//start time of profiling
struct timeval finish;//stop time of profiling
bool profile_data = false, profile_inst = false;


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

VOID Record(ADDRINT ref){
  stamp_t i, idx;

  N ++; 
  for(i = 0; i < 2; i ++){
    if (!i) {
      idx = Index1(ref);
    }else {
      idx = Index2(ref);
    }
    stamp_tt prev_access = stamps[idx][i];
    stamps[idx][i] = N;

    idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS> (N-prev_access);

    if(!prev_access){
      M[i] ++;
      first_count_i[idx][i] += N - prev_access;
      first_count[idx][i] += 1;
    }else{
      count_i[idx][i] += N - prev_access;
      lcount[idx][i] += 1;
    }
  }

  //  if(( N >= threshold) || (M >= cache_threshold))
  //    {
  //    PIN_ExitApplication(0);
  //  }

}


VOID RecordMem(VOID * ip, VOID * addr)
{
  Record((ADDRINT)addr);
}

VOID RecordInst(VOID *ip)
{
  Record((ADDRINT)ip);
}


VOID Instruction(INS ins, VOID *v) {
    // Instruments loads using a predicated call, i.e.
    // the call happens iff the load will be actually executed.
    // The IA-64 architecture has explicitly predicated instructions. 
    // On the IA-32 and Intel(R) 64 architectures conditional moves and REP 
    // prefixed instructions appear as predicated instructions in Pin.
  if(profile_data){
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

  if(profile_inst){
    // Insert a call to printip before every instruction, and pass it the IP
    INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)RecordInst, IARG_INST_PTR, IARG_END);
  }
}



/* ===================================================================== */

VOID Fini(INT32 code, VOID *v){
  
  gettimeofday(&finish, 0);
  stamp_t time = (finish.tv_sec - start.tv_sec);
  stamp_t i, idx, k;
  stamp_tt ws, tmp;
  
  char fname[512]; //[50]; //need to support long filename
  char mode = 'd';
  if(profile_inst){
    if(profile_data) mode = 'a';
    else mode = 'i';
  }
  sprintf(fname, "%s.%c", KnobResultFile.Value().c_str(), mode);

  ResultFile.open(fname);
  ResultFile << "N:" << N << " M1:" << M[0] << " M2:" << M[1] << " total_time:" << time  << endl;
  
  for(k = 0; k < 2; k++) {
    for(i = 0; i < MAP_SIZE; i++){
      if(!stamps[i][k])
	continue;
      tmp = N-stamps[i][k] + 1;
      idx = sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(tmp);
      first_count[idx][k] += 1;
      first_count_i[idx][k] += tmp;
    }
  }

  double sum=0.0, sum_i=0.0, fp, mr=1.0;
  double sum2=0.0, sum_i2=0.0, fp2, mr2=1.0;
  
  for(i=1;i<=sublog_value_to_index<MAX_WINDOW, SUBLOG_BITS>(N);i++){
    ws = sublog_index_to_value<MAX_WINDOW, SUBLOG_BITS>(i);
    fp = ( 1.0 * (( N - sum ) * ws + sum_i)) / ( N - ws + 1 );
    sum_i += count_i[i][0] + first_count_i[i][0];
    sum += lcount[i][0] + first_count[i][0];
    mr -= lcount[i][0] * 1.0 / N;
    ResultFile << i << "\t" << ws << "\t" << fp*(1<<WORDSHIFT1) << "\t" <<  lcount[i][0]*1.0/N << "\t" << mr << "\t";

    fp2 = ( 1.0 * (( N - sum2 ) * ws + sum_i2)) / (N - ws + 1);
    sum_i2 += count_i[i][1] + first_count_i[i][1];
    sum2 += lcount[i][1] + first_count[i][1];
    mr2 -= lcount[i][1] * 1.0 / N;
    ResultFile << fp2*(1<<WORDSHIFT2) << "\t" <<  lcount[i][1] * 1.0 / N << "\t" << mr2 << endl;
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

    UINT mode = KnobProfileMode.Value();
    if(mode & 1)
      profile_data = true;
    if(mode & 2)
      profile_inst = true;
    memset (stamps, 0, MAP_SIZE*sizeof(stamp_t)*2);
    memset (lcount, 0, MAX_WINDOW*sizeof(stamp_tt)*2);
    memset (count_i, 0, MAX_WINDOW*sizeof(stamp_tt)*2);
    memset (first_count, 0, MAX_WINDOW*sizeof(stamp_tt)*2);
    memset (first_count_i, 0, MAX_WINDOW*sizeof(stamp_tt)*2);
    M[0] = 0;
    M[1] = 0;

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
