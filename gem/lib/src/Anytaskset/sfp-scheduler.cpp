#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <map>
#include <sys/time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "pin.H"
#include "common.H"
#include "portability.H"
#include "histo.H"
#include "rdtsc.H"
#include "atomic.H"
#include "instlib.H"
#include "sfp_list.H"
#include "sfp_tokens.H"

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
#define MAX_TOKENS 26     // max token supported

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

/* control variable */
LOCALVAR CONTROL control;


#include "thread_support_scheduler.H"

/* ===================================================================== */
/* Routines */
/* ===================================================================== */


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
// The replacing routine for SFP_TaskStart
//
void BeforeTaskStart(unsigned int tid, unsigned int parent) {
  
  local_stat_t* lstat = get_tls(PIN_ThreadId());
  
  lstat->tasks.push_back(tid);
  lstat->current_task = tid;

}

//
// The replacing routine for SFP_TaskEnd
//
void BeforeTaskEnd(int tid) {

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

    /* check for knobs if region instrumentation is involved */
    control.RegisterHandler(ControlHandler, 0, FALSE);
    control.Activate();

    /* register callbacks */
    PIN_AddThreadStartFunction(ThreadStart, 0);
    PIN_AddThreadFiniFunction(ThreadFini, 0);
    IMG_AddInstrumentFunction(ImageLoad, 0);

    /* init thread hooks, implemented in thread_support.H */
    ThreadInit();

    // Never returns
    PIN_StartProgram();
    
    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */

