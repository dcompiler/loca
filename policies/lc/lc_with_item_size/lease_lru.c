/**
 * ++ C ++
 *
 * Not using C++, C is more effective.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#include "static.h"
#include "hash.h"
#include "dll.h"
#include "attrs.h"
#include "lru_cache.h"
#include "lease_bins.h"
#include "central_storage.h"
#include "lease_lru.h"
#include "workload.h"

#ifdef __APPLE__
#define MAP_ANONYMOUS MAP_ANON
#endif

#define MAX_TRACE_LENGTH 80000000
#define TRACE_THRESHOLD 8000000
/* #define MAX_TRACE_LENGTH 8000000 */

/* all lee caches */
LLeeCache_t * lee_caches[NUM_SLAB_CLASS+1] = {NULL};

/* item_num */
uint32_t item_num[NUM_SLAB_CLASS + 1] = {0};

/* base_size */
uint32_t base_size[NUM_SLAB_CLASS + 1];

/* miss ration */
/* double *missRatios = (double *)malloc(sizeof(double) * MAX_MR_INTERVAL); */
double missRatios[MAX_MR_INTERVAL];
uint32_t totalHits = 0;
uint32_t totalMisses = 0;

/* count average cache size */
double realAvgCacheSize[NUM_SLAB_CLASS + 1] = {0};

/* use lease cache policy or lama */
int policy = 0;

int timer = 0;

LWorkload_t workloads[] = {
    {
        "fb3",
        15000000, 
        50000010, 
        "../../../traces/key_slab",
        "../../../traces/fb/trace3/key_lease",
        "../../../traces/fb/trace3/trace.tr",
        "../../../traces/item_num",
    },
    {
        "fb4",
        15000000, 
        50000010, 
        "../../../traces/key_slab",
        "../../../traces/fb/trace4/key_lease",
        "../../../traces/fb/trace4/trace.tr",
        "../../../traces/item_num",
    },
    {
        "fb5",
        15000000, 
        50000010, 
        "../../../traces/key_slab",
        "../../../traces/fb/trace5/key_lease",
        "../../../traces/fb/trace5/trace.tr",
        "../../../traces/item_num",
    },
    {
        "fb6",
        15000000, 
        50000010, 
        "../../../traces/key_slab",
        "../../../traces/fb/trace6/key_lease",
        "../../../traces/fb/trace6/trace.tr",
        "../../../traces/item_num",
    },
    {
        "twi2",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/twi/trace2/key_lease",
        "../../../traces/twi/trace2/trace.tr",
        "../../../traces/item_num",
    },
    {
        "twi5",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/twi/trace5/key_lease",
        "../../../traces/twi/trace5/trace.tr",
        "../../../traces/item_num",
    },
    {
        "hm_0",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/hm_0/key_lease",
        "../../../traces/msr/hm_0/trace.tr",
        "../../../traces/item_num",
    },
    {
        "hm_2",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/hm_2/key_lease",
        "../../../traces/msr/hm_2/trace.tr",
        "../../../traces/item_num",
    },
    {
        "prn_0",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/prn_0/key_lease",
        "../../../traces/msr/prn_0/trace.tr",
        "../../../traces/item_num",
    },
    {
        "prn_1",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/prn_1/key_lease",
        "../../../traces/msr/prn_1/trace.tr",
        "../../../traces/item_num",
    },
    {
        "proj_0",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/proj_0/key_lease",
        "../../../traces/msr/proj_0/trace.tr",
        "../../../traces/item_num",
    },
    {
        "proj_1",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/proj_1/key_lease",
        "../../../traces/msr/proj_1/trace.tr",
        "../../../traces/item_num",
    },
    {
        "proj_4",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/proj_4/key_lease",
        "../../../traces/msr/proj_4/trace.tr",
        "../../../traces/item_num",
    },
    {
        "src1_0",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/src1_0/key_lease",
        "../../../traces/msr/src1_0/trace.tr",
        "../../../traces/item_num",
    },
    {
        "src1_1",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/src1_1/key_lease",
        "../../../traces/msr/src1_1/trace.tr",
        "../../../traces/item_num",
    },
    {
        "mds_0",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/mds_0/key_lease",
        "../../../traces/msr/mds_0/trace.tr",
        "../../../traces/item_num",
    },
    {
        "mds_1",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/mds_1/key_lease",
        "../../../traces/msr/mds_1/trace.tr",
        "../../../traces/item_num",
    },
    {
        "stg_0",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/stg_0/key_lease",
        "../../../traces/msr/stg_0/trace.tr",
        "../../../traces/item_num",
    },
    {
        "stg_1",
        8000000,
        50000000,
        "../../../traces/key_slab",
        "../../../traces/msr/stg_1/key_lease",
        "../../../traces/msr/stg_1/trace.tr",
        "../../../traces/item_num",
    },
};

LWorkload_t * start_simulator(char * workload)
{
    int32_t N, M;
    LWorkload_t * ret = NULL;
    int i;
    for ( i = 0; i < sizeof(workloads)/sizeof(LWorkload_t); i ++ ) {
        if ( strcmp( workloads[i].name, workload ) == 0 ) {
            ret = &workloads[i];
            M = workloads[i].M; 
            N = workloads[i].N;
            key_slab_map = (int32_t *)malloc(sizeof(int32_t) * (M+1) );
            memset(key_slab_map, 0 , sizeof(int32_t) * (M+1) );
            key_lease_map = (int32_t *)malloc(sizeof(int32_t) * (M+1) );
            memset(key_lease_map, 0 , sizeof(int32_t) * (M+1) );
            break;
        }
    }

    if ( ret == NULL ){
        printf("[ERR] cannot find workload.\n");
        exit(-1);
    }

    lee_caches[0] = NULL; /* no lee_cache allocated for slab class 0, which doesn't exist. */
    int j;


    for ( j = 1; j <= NUM_SLAB_CLASS; j ++ ) {
        /* lee_caches[j] = create_lee_cache( j, CACHE_BASE_SIZE, MAX_LEASE_TIME, &workloads[i] ); */
        /* lee_caches[j] = create_lee_cache(j, base_size[j], MAX_LEASE_TIME, &workloads[i]); */
        lee_caches[j] = create_lee_cache(j, 100000, MAX_LEASE_TIME, &workloads[i]);
    }

    return ret;
}

/* split start_simulator to two functions */
LWorkload_t *initWorkload(char *workload) {
    int32_t N, M;
    LWorkload_t * ret = NULL;
    int i;
    for ( i = 0; i < sizeof(workloads)/sizeof(LWorkload_t); i ++ ) {
        if ( strcmp( workloads[i].name, workload ) == 0 ) {
            ret = &workloads[i];
            M = workloads[i].M; 
            N = workloads[i].N;
            key_slab_map = (int32_t *)malloc(sizeof(int32_t) * (M+1) );
            memset(key_slab_map, 0 , sizeof(int32_t) * (M+1) );
            key_lease_map = (int32_t *)malloc(sizeof(int32_t) * (M+1) );
            memset(key_lease_map, 0 , sizeof(int32_t) * (M+1) );

            /* create accesses */
            accesses = (uint32_t *)malloc(sizeof(uint32_t) * (M + 1));
            memset(accesses, 0, sizeof(uint32_t) * (M + 1));

            break;
        }
    }

    if ( ret == NULL ){
        printf("[ERR] cannot find workload.\n");
        exit(-1);
    }
    return ret;
}

void initLeeCache(LWorkload_t * workload) {
    lee_caches[0] = NULL; /* no lee_cache allocated for slab class 0, which doesn't exist. */
    int j;
    for ( j = 1; j <= NUM_SLAB_CLASS; j ++ ) {
        /* lee_caches[j] = create_lee_cache( j, CACHE_BASE_SIZE, MAX_LEASE_TIME, &workloads[i] ); */
        if (policy == 1) {
            /* lama */
            lee_caches[j] = create_lee_cache(j, base_size[j], MAX_LEASE_TIME, workloads);
        } else {
            /*lease cache */
            lee_caches[j] = create_lee_cache(j, 0, MAX_LEASE_TIME, workloads);
        }
    }
}

void stop_simulator( LWorkload_t * workload )
{
    if ( key_slab_map ) free( key_slab_map );
    if ( key_lease_map ) free( key_lease_map );

    int i;
    for ( i = 1; i <= NUM_SLAB_CLASS; i ++ ) {
        /* destroy_lee_cache( lee_caches[i] ); */
    }
}

void do_access(int key, int slab_class)
{
    if ( slab_class > NUM_SLAB_CLASS ) {
        printf("[WARN] slab class greater than num_slab_classes defined. \n");
        return;
    }

    global_timer ++;
    local_timers[slab_class] ++;
    //key is 1, lee_caches[0]
    //printf("hey");
    //exit(0);
    lee_cache_op_entrance( lee_caches[slab_class], key );

}

void read_access_trace(char * name)
{
    FILE * file_hd;
    if ( ! ( file_hd = fopen(name, "rb") ) ) {
        printf("file %s not exist.\n", name);
        return;
    }
//    printf("[LOG]reading key access trace %s ...\n", name);
//
    


    int l;
    int i = 0;
    int j = 0;
    char buf[1024];
    int key, slab_class;
    int missRationPoint = 0;

    /* start process trace from 8 million + 1 line */
    /* while( fgets(buf, sizeof(buf), file_hd) && i++ < TRACE_THRESHOLD); */

    while( fgets(buf, sizeof(buf), file_hd) && i++ < MAX_TRACE_LENGTH) {
        char * p = buf;
        key = atoi(p);

        assert( key != 0 );

        //slab_class = key_slab_map[key];

        slab_class = 1;
        //printf("Key: %d\n", key);
        //printf("Slab_class: %d\n", slab_class);

        do_access(key, slab_class); // seg fault here

        memset(buf, 0, 1024);

        timer++;

        /* if (i % (10 * MAX_MR_INTERVAL) == 0) { */
        /*     missRatios[missRationPoint++] = ((double)totalMisses) / ((double)global_timer); */
        /*     printf("miss ratio = %.8f\n", missRatios[missRationPoint - 1]); */
        /* } */

        for (j = 1; j < NUM_SLAB_CLASS + 1; j++) {
            /* realAvgCacheSize[j] += lee_caches[j]->lease_comp->size; */
            realAvgCacheSize[j] += lee_caches[j]->lease_comp->size + lee_caches[j]->lru_comp->size;
            if (lee_caches[j]->lru_comp->size != 0) printf("SCREAM!!!\n");
        }

        /* printf("lease_com size = %d\n", lee_caches[1]->size); */

        /* if (i % (10 * MAX_MR_INTERVAL) == 0) { */
        /*     printf("LRU size = %d\n", lee_caches[1]->lru_comp->size); */
        /* } */

    }
    fclose(file_hd);
}

void read_key_slab_file(char * name)
{
    FILE * file_hd;
    if ( ! ( file_hd = fopen(name, "rb") ) ) {
        printf("file %s not exist.\n", name);
        return;
    }
    //printf("[LOG]reading key_slab file %s ...\n", name);

    int i=1; /* key counts from 1. */
    char buf[1024];
    int slab_class;
    while( fgets(buf, sizeof(buf), file_hd) ) {
        char * p = buf;
        //printf("p: %s\n", p);
        //while ( *p != ' ' ) p++;
        //p++;
        //printf("p: %s\n", p);
        slab_class = atoi(p);
        //if (slab_class == NULL) {
            //printf("hey");
        //}
        key_slab_map[i] = slab_class;
        i++;

        memset(buf, 0, 1024);
    }

    fclose(file_hd);
}

void read_lease_file(char * name)
{
    FILE * file_hd;
    if ( ! ( file_hd = fopen(name, "rb") ) ) {
        printf("file %s not exist.\n", name);
        return;
    }
    //printf("[LOG]reading lease file %s ...\n", name);
  
    int i=1; /* key counts from 1. */
    char buf[1024];
    int lease;
    while( fgets(buf, sizeof(buf), file_hd) ) {
        char * p = buf;
        lease = atoi(p);
        key_lease_map[i++] = lease;

        memset(buf, 0, 1024);
    }

    fclose(file_hd);
}

void read_item_num_file(char *name) {
    FILE * file_hd;
    if (!(file_hd = fopen(name, "rb"))) {
        printf("file %s not exists.\n", name);
        return;
    }

    char buf[1024];
    int class = 1;
    while (fgets(buf, sizeof(buf), file_hd)) {
        item_num[class++] = atoi(buf);
    }
    fclose(file_hd);
}

void read_slab_mem_file(char *name) {
    FILE *file;
    if (!(file = fopen(name, "rb"))) {
        printf("file %s not exists.\n", name);
        return;
    }

    char buf[1024];
    int class = 1;
    while (fgets(buf, sizeof(buf), file)) {
        base_size[class] = item_num[class] * atoi(buf);
        class++;
        /* slab_mem[class++] = atoi( */
    }
    fclose(file);
}

void write_mr_to_file(char *name) {
    char traceName[100];
    strcpy(traceName, name);
    /* char fileNamePrefix[100] = "../workloads/"; */
    char fileNamePrefix[100] = "../../traces/";
    char *fileNamePostfix = "/miss_ratio";
    strcat(traceName, fileNamePostfix);
    strcat(fileNamePrefix, traceName);
    /* printf("file name: %s\n", fileNamePrefix); */
    /* printf("file name: %s\n", name); */
    FILE *file = fopen(fileNamePrefix, "w");
    int i = 0;
    for (; i < (MAX_TRACE_LENGTH / MAX_MR_INTERVAL); i++) {
        fprintf(file, "%d %.6f\n",  (i + 1) * MAX_MR_INTERVAL, missRatios[i]);
    }
    fclose(file);
}

void computeTotalMemory(char *name) {
    int i = 1;
    for (; i < NUM_SLAB_CLASS + 1; i++) {
        if (item_num[i] != 0) {
            realAvgCacheSize[i] = realAvgCacheSize[i] / global_timer / ((float)item_num[i]);
        } else {
            realAvgCacheSize[i] = 0;
        }
    }

    FILE *realSlabMem = fopen("mrc_mem_data", "w");

    double totalMem = 0;
    /* double totalMiss = 0; */
    printf("Total slab: %d\n", NUM_SLAB_CLASS);
    for (i = 1; i < NUM_SLAB_CLASS + 1; i++) {
        fprintf(realSlabMem, "%d %d %.5f %.5f\n", i, lee_caches[i]->stats.misses,  ((float)(lee_caches[i]->stats.misses)) / ((float)(lee_caches[i]->stats.accesses)), realAvgCacheSize[i]);
        /* totalMiss += lee_caches[i]->stats.misses; */
        totalMem += realAvgCacheSize[i];
        printf("there are still %d items alive\n", lee_caches[i]->lease_comp->size);
    }
    fprintf(realSlabMem, "\n%.5f %.5f\n", totalMem, ((float)totalMisses) / MAX_TRACE_LENGTH );
    fclose(realSlabMem);
    printf("total memory = %.4f\n", totalMem);
    printf("total misses = %d\n", totalMisses);
    printf("final mr = %.4f\n", (float)(totalMisses) / (float)(timer));
}

void usage()
{
    printf("Usage:\n");
    printf("\t./lee_cache_simulator mode workload\n");
    printf("\tmode:\n");
    printf("\t\t0 -- get reuse distance histogram.\n");
    printf("\tworkload:\n");
    printf("\t\tfacebook -- the facebook workload.\n");
    printf("\t\t    test -- the test workload.\n");
}

int main(int argc, char *argv[])
{
    if ( argc < 3 ) { usage(); return 0; }

    if (argc == 4 && atoi(argv[3]) == 1) policy = 1; /* lama */

    /* record running time */
    time_t startTime = time(NULL);
    printf("%s", ctime(&startTime));

    int mode = atoi(argv[1]);
    char * workload = argv[2];
    if ( workload == NULL ) {
        printf("[ERR] workload input cannot be null.\n");
        usage();
        exit(-1);
    }

    /* LWorkload_t * workload_data = start_simulator(workload); */
    LWorkload_t * workload_data = initWorkload(workload);

    int i;
    switch(mode) {
    case 0:
        //printf("hello\n");
        read_key_slab_file(workload_data->key_slab_file);
        read_lease_file(workload_data->key_lease_file);
        read_item_num_file(workload_data->item_num_file);
        /* read_slab_mem_file(workload_data->slab_mem_file); */
        initLeeCache(workload_data);
        read_access_trace(workload_data->trace_file);
        /* write_mr_to_file(workload); */
        computeTotalMemory(workload);
        break;
    case 1:
        break;
    default:
        printf("Error: unknown mode!\n");
        usage();
    }

Exit:
    stop_simulator(workload_data);

    time_t endTime = time(NULL);
    printf("%s\n", ctime(&endTime));
    
    return 1;
}


