#ifndef _L_WORKLOAD_T_
#define _L_WORKLOAD_T_

/* workload_t */
struct _LWorkload_t {
    char name[1024];
    int32_t M; /* number of keys */
    int32_t N; /* trace length */
    char key_slab_file[1024];
    char key_lease_file[1024];
    char trace_file[1024];
    char item_num_file[1024];
    char slab_mem_file[1024];
};

#endif

/* LWorkload_t workloads[] = { */
/*     { */
/*         "fb3", */
/*         15000000, */ 
/*         50000010, */ 
/*         "../../../traces/key_slab", */
/*         "../../../traces/fb/trace3/key_lease", */
/*         "../../../traces/fb/trace3/trace.tr", */
/*         "../../../traces/item_num", */
/*     }, */
/*     { */
/*         "fb4", */
/*         15000000, */ 
/*         50000010, */ 
/*         "../../../traces/key_slab", */
/*         "../../../traces/fb/trace4/key_lease", */
/*         "../../../traces/fb/trace4/trace.tr", */
/*         "../../../traces/item_num", */
/*     }, */
/*     { */
/*         "fb5", */
/*         15000000, */ 
/*         50000010, */ 
/*         "../../../traces/key_slab", */
/*         "../../../traces/fb/trace5/key_lease", */
/*         "../../../traces/fb/trace5/trace.tr", */
/*         "../../../traces/item_num", */
/*     }, */
/*     { */
/*         "twi2", */
/*         8000000, */
/*         50000000, */
/*         "../../../traces/key_slab", */
/*         "../../../traces/twi/trace2/key_lease", */
/*         "../../../traces/twi/trace2/trace.tr", */
/*         "../../../traces/item_num", */
/*     }, */
/*     { */
/*         "twi3", */
/*         8000000, */
/*         50000000, */
/*         "../../../traces/key_slab", */
/*         "../../../traces/twi/trace3/key_lease", */
/*         "../../../traces/twi/trace3/trace.tr", */
/*         "../../../traces/item_num", */
/*     }, */
/*     { */
/*         "hm_0", */
/*         8000000, */
/*         50000000, */
/*         "../../../traces/key_slab", */
/*         "../../../traces/msr/hm_0/key_lease", */
/*         "../../../traces/msr/hm_0/trace.tr", */
/*         "../../../traces/item_num", */
/*     }, */
/*     { */
/*         "prn_0", */
/*         8000000, */
/*         50000000, */
/*         "../../../traces/key_slab", */
/*         "../../../traces/msr/prn_0/key_lease", */
/*         "../../../traces/msr/prn_0/trace.tr", */
/*         "../../../traces/item_num", */
/*     }, */
/*     { */
/*         "prn_1", */
/*         8000000, */
/*         50000000, */
/*         "../../../traces/key_slab", */
/*         "../../../traces/msr/prn_1/key_lease", */
/*         "../../../traces/msr/prn_1/trace.tr", */
/*         "../../../traces/item_num", */
/*     }, */
/* }; */


