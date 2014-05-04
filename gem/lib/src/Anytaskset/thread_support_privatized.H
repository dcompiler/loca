/* This file provide thread support to sample fp tool
 * It includes the thread local data def and some global
 * state information.
 *
 * To implement thread accounting, just add method and state in 
 * local_stat_t class and call them in sample_fp.cpp RecordMem file
 *
 * Written by Hao Luo
 *
 */

#ifndef _THREAD_SUPPORT_PRIVATIZED_H_
#define _THREAD_SUPPORT_PRIVATIZED_H_

#include <vector>
#include "pin.H"

using namespace std;

/* ======================================= */
/* Global thread variables */
/* ======================================= */

// thread-local-storage key
static TLS_KEY tls_key;

// lock used to protect gThreadNum
static PIN_LOCK thd_num_lock;

// thread count
unsigned gThreadNum = 0;

/* ======================================= */
/* Data structure */
/* ======================================= */

/* thread local data */
struct local_stat_t {

  /* Add thread local information and updating method here */
  bool enabled;

  uint64_t last;

  INT64 wcount[MAX_THREAD][MAX_WINDOW];
  INT64 wcount_i[MAX_THREAD][MAX_WINDOW];
  INT64 per_wcount[MAX_WINDOW];
  INT64 per_wcount_i[MAX_WINDOW];

  uint64_t M[MAX_THREAD];
  uint64_t per_M;

  map<TBitset, TStamp> pillars[MAX_PILLARS];

  vector<int> tasks;
  int current_task; 

  TStamp length;
  TStamp accum_time;

  local_stat_t() : enabled(false),
                   last(0),
                   per_M(0),
                   current_task(0),
                   length(0),
                   accum_time(0)
  {
    tasks.push_back(0);
    for(int i=0; i<MAX_THREAD; i++)
    {

      for(TStamp j=0; j<MAX_WINDOW; j++)
      {
        wcount[i][j] = 0;
        wcount_i[i][j] = 0;
      }
      M[i] = 0;;

    }
  }
};

/* ======================================= */
/* Functions */
/* ======================================= */

inline void ThreadStart_hook(THREADID tid, local_stat_t* tdata);
inline void ThreadFini_hook(THREADID tid, local_stat_t* tdata);

/* interface to get thread local data */
inline local_stat_t* get_tls(THREADID tid) {
  return static_cast<local_stat_t*>(PIN_GetThreadData(tls_key, tid));
}

/* hook at thread spawn */
VOID ThreadStart(THREADID tid, CONTEXT* ctxt, INT32 flags, VOID* v) {
 
  PIN_GetLock(&thd_num_lock, tid+1);
  gThreadNum++;
  PIN_ReleaseLock(&thd_num_lock);
 
  local_stat_t* tdata = new local_stat_t;
  PIN_SetThreadData(tls_key, tdata, tid);

  ThreadStart_hook(tid, tdata);

}

/* hook at thread end */
VOID ThreadFini(THREADID tid, const CONTEXT* ctxt, INT32 flags, VOID* v) {

  local_stat_t* tdata = get_tls(tid);
  ThreadFini_hook(tid, tdata);  

}

/* initializing thread local data, must be called in main */
VOID ThreadInit() {
  
  // Initialize tls storeage
  tls_key = PIN_CreateThreadDataKey(0);
  PIN_InitLock(&thd_num_lock);

}

/* deallocation of thread local data */
VOID ThreadEnd() {
  for(unsigned int i=0;i<gThreadNum;i++) {
     local_stat_t* tdata = get_tls(i);
     delete tdata;
  }
}

#endif
