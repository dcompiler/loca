
/**
 * The current version records abundant information, e.g., lease_or_lru of 
 * an item, sizes of lru cache and lease cache. These information is for data 
 * modeling. For performance, we can remove those and implement a much faster 
 * version (same O(1) efficiency, but a much smaller constant coefficient). 
 */

#ifndef _L_LEASE_LRU_H_
#define _L_LEASE_LRU_H_

#include "hash.h"
#include "dll.h"
#include "attrs.h"
#include "lru_cache.h"
#include "lease_bins.h"
#include "central_storage.h"
#include "declarations.h"
#include "item.h"
#include "workload.h"

#define MAX_LEASE_TIME (8000000)
#define FAKED_ITEM_KEY_ID_START (-1)
#define MAX_NUM_ITEM   (15000005)
#define CACHE_BASE_SIZE (600000)

#define MAX_RD 600000

#define MAX_MR_INTERVAL 10000

#define CACHE_STATS
#undef  CACHE_DEBUG
/**
 * define either MRC_PROFILING or REUSE_DISTANCE_PROFILING 
 * or AVERAGE_CACHE_SIZE_PROFILING
 */
#define  MRC_PROFILING
#undef REUSE_DISTANCE_PROFILING
#undef AVERAGE_CACHE_SIZE_PROFILING
/**
 * means that reuse distance profiling requires average cache size profiling. 
 * but, average cache size profiling is of interest also, alone.
 */

#undef MRC_PROFILING
#if defined(MRC_PROFILING)
#define REUSE_DISTANCE_PROFILING
#define AVERAGE_CACHE_SIZE_PROFILING
#elif defined(REUSE_DISTANCE_PROFILING)
#define AVERAGE_CACHE_SIZE_PROFILING 
#endif

typedef enum {LRU_ONLY, LEASE_ONLY, LEASE_LRU} CACHE_TYPE;

struct _LStats_t{
    int64_t accesses;
    int64_t hits;
    int64_t misses;
}ATTR_ALIGN_TO_AVOID_FALSE_SHARING;

/**
 * Currently, only support grow factor of 1. To support 1M, the ``size" 
 * calculations should be changed.
 */
typedef struct _LLeaseLRUCache_t {
    LLRUCache_t          *lru_comp;
    LOrderedLeaseCache_t *lease_comp;

    int32_t              slab_class;
    int32_t              chunk_size;
    CACHE_TYPE           cache_type;
    size_t               base_size;   /* MAX_NUM_ITEM shows that cache infinitely grows. */
    int32_t              grow_factor; /* 1 default case, for modeling; 1M memcached case. */
    size_t               size;        /* realtime size */
    int32_t              max_lease_time;

    HashTable_t          *hash_table;

    LCentralStorage_t    central_storage;
    LStats_t             stats;

    /* twitter's way */
    int32_t currentMemory;
    int32_t itemNumInCurrent1M;

#if defined(REUSE_DISTANCE_PROFILING)
    int32_t              *rd_hist;
#endif
#if defined(AVERAGE_CACHE_SIZE_PROFILING)
    int32_t              *live_objs_hist; /* for computing average cache size. */
    double               *ave_cache_size;
#endif

    LWorkload_t          *workload;
} LLeaseLRUCache_t;

extern uint32_t item_num[NUM_SLAB_CLASS + 1];

/* declare an array to store miss ration */
extern double missRatios[MAX_MR_INTERVAL];
extern uint32_t totalHits;
extern uint32_t totalMisses; 


#define TRACE_LENGTH_THRESHOLD 8000000

/* ----- item related functions ----- */

LItem_t * search_item_in_lee_cache ( LLeeCache_t * lee_cache, int64_t key_id )
{
    LItem_t * item = HashTableGet ( lee_cache->hash_table, key_id );
    return item;
}

/* true - in lru; false - in lease */
bool is_item_in_lru( LItem_t * item )
{
    return item->lease_or_lru == LRU; 
}

/* true - in lease; false - in lru */
bool is_item_in_lease( LItem_t * item )
{
    return item->lease_or_lru == LEASE;
}

LItem_t * alloc_item( int64_t key_id, int32_t lease, POSITION lease_or_lru )
{
    LItem_t * item = (LItem_t *)malloc(sizeof(LItem_t));
    item->key_id = key_id;
    item->value = 0;
    item->lease_time = lease;
    item->lease_or_lru = lease_or_lru; 
    item->hash_node = NULL;

    return item;
}

void update_item( LItem_t * item, int64_t key_id, int32_t lease, 
        POSITION lease_or_lru )
{
    item->key_id = key_id;
    item->lease_time = lease;
    item->lease_or_lru = lease_or_lru;
}

void unlink_from_hash_table(LItem_t *);
void delete_item( LItem_t * item )
{
    unlink_from_hash_table( item );

    free( item );
}

void link_to_hash_table( LLeeCache_t * lee_cache, LItem_t * item )
{
    HashTableElem_t * new_hash_node =
        (HashTableElem_t *) malloc ( sizeof(HashTableElem_t) );
    new_hash_node->key_id = item->key_id;
    new_hash_node->points_to = item;
    INIT_HLIST_NODE(&new_hash_node->list);
    HashTableAdd( lee_cache->hash_table, new_hash_node );

    item->hash_node = new_hash_node;
}

void relink_to_hash_table( LLeeCache_t * lee_cache, LItem_t * item )
{
    HashTableElem_t * elem = item->hash_node;
    hlist_del( &elem->list );
    elem->key_id = item->key_id;
    HashTableAdd( lee_cache->hash_table, elem );
}

void unlink_from_hash_table( LItem_t * item ) 
{
    HashTableElem_t * elem = item->hash_node;
    hlist_del( &elem->list );
    free( elem );
}

/* ----- lee_cache related functions ----- */

int32_t is_lru_component_empty( LLeeCache_t * lee_cache )
{
    return lru_cache_empty( lee_cache->lru_comp );
}

/* create a lee cache */
LLeeCache_t * create_lee_cache( int32_t slab_class, size_t base_size, 
        int32_t max_lease_time, LWorkload_t * workload )
{
    LLeeCache_t * lee_cache = (LLeeCache_t *)malloc(sizeof(LLeeCache_t));
    memset(lee_cache, 0, sizeof(LLeeCache_t));

    lee_cache->slab_class = slab_class;
    lee_cache->chunk_size = slab_class_to_chunk_size(slab_class);

    lee_cache->base_size  = base_size;
    const char * env = getenv("LEE_CACHE_SIZE");
    if ( env != 0  && atoi(env) != 0 ) {
        lee_cache->base_size = atoi(env);
    }

    lee_cache->cache_type = LEASE_LRU;
    lee_cache->grow_factor = 1;
    lee_cache->size = lee_cache->base_size;
    lee_cache->max_lease_time = max_lease_time;
    lee_cache->workload = workload;
    if ( lee_cache->workload->M > MAX_NUM_ITEM ) {
        fprintf(stderr, "[ERR] workload.M greater than MAX_NUM_ITEM.\n");
        assert(0);
    }

    /* create lru component */
    lee_cache->lru_comp = create_lru_cache( 0, slab_class );

    /* create lease component */
    lee_cache->lease_comp = 
        create_ordered_lease_cache( 0, lee_cache->max_lease_time, slab_class );

    /* init hash table */
    lee_cache->hash_table = create_hash_table();

    /** 
     * create the faked-item structure -
     *   at start, assign all base_size to lru component.
     *   so the passing parameter is base_size to lru_cache_creation.
     */
    int64_t countdown_key = FAKED_ITEM_KEY_ID_START;
    size_t i;
    for ( i = 0; i < lee_cache->base_size; i++ ) {

        LItem_t * new_item = alloc_item( countdown_key--, 0, LRU );

        link_to_hash_table( lee_cache, new_item );

        add_to_lru_cache( lee_cache->lru_comp, new_item );
    }

    /* init central storage */
    memset( &lee_cache->central_storage, 0, sizeof(LCentralStorage_t) );

    memset( &lee_cache->stats, 0, sizeof(LStats_t) );

#if defined(REUSE_DISTANCE_PROFILING)
    lee_cache->rd_hist = (int32_t *)malloc(sizeof(int32_t) * (MAX_NUM_ITEM+1) );
    memset( lee_cache->rd_hist, 0, sizeof(int32_t) * (MAX_NUM_ITEM+1) );
#endif
#if defined(AVERAGE_CACHE_SIZE_PROFILING)
    lee_cache->live_objs_hist = (int32_t *)malloc(sizeof(int32_t) * (MAX_NUM_ITEM+1) );
    memset( lee_cache->live_objs_hist, 0, sizeof(int32_t) * (MAX_NUM_ITEM+1) );

    lee_cache->ave_cache_size = (double *)malloc(sizeof(double) * (MAX_NUM_ITEM+1) );
    memset( lee_cache->ave_cache_size, 0, sizeof(double) * (MAX_NUM_ITEM+1) );
#endif

    return lee_cache;
}

#if defined(MRC_PROFILING)
void lee_cache_stats( LLeeCache_t * lee_cache );
/* destroy a lee cache */
void destroy_lee_cache( LLeeCache_t * lee_cache )
{
#if defined(CACHE_STATS)
    lee_cache_stats( lee_cache );
#endif
    destroy_lru_cache( lee_cache->lru_comp );

    destroy_ordered_lease_cache( lee_cache->lease_comp );

    destroy_hash_table( lee_cache->hash_table );

#if defined(REUSE_DISTANCE_PROFILING)
    free( lee_cache->rd_hist );
#endif
#if defined(AVERAGE_CACHE_SIZE_PROFILING)
    free( lee_cache->live_objs_hist );
    free( lee_cache->ave_cache_size );
#endif

    free( lee_cache );
}

void compute_average_cache_size( int32_t *live_objs_hist, double *ave_cache_size )
{
    int i;
    int32_t M = MAX_NUM_ITEM;
    int64_t N = 0; 
    int32_t * live_objs_hist_temp = (int32_t *)malloc( sizeof(int32_t) * (MAX_NUM_ITEM + 1) );
    memcpy( live_objs_hist_temp, live_objs_hist, sizeof(int32_t) * (MAX_NUM_ITEM + 1) );

    for ( i = 0; i <= M; i ++ ) {
        N += live_objs_hist_temp[i];
    }

    double sum = 0;
    sum += ( (double)live_objs_hist_temp[M] ) / N * M;  
    ave_cache_size[M] = M;
    for ( i = M-1; i >= 0; i -- ) {
        ave_cache_size[i] = ( (double)(N - live_objs_hist_temp[i+1]) / N * i ) + sum;
        /* sum += ( (double)live_objs_hist_temp[i] ) / N * i; */
        sum += ((double)live_objs_hist[i]) / N * i;
        live_objs_hist_temp[i] += live_objs_hist_temp[i+1];
    }

    /* int what = 0; */
    /* for (i = 0; i < 100000; i++) { */
    /*     if(live_objs_hist_temp[i] == live_objs_hist[i]) { */
    /*         what++; */
    /*     } */
    /* } */
    /* printf("what is %d ???\n", what); */

    free(live_objs_hist_temp);
}

void print_average_cache_size( LLeeCache_t * lee_cache )
{
    /**
     * TODO: only output the first 10001 terms for the time being.
     */
    int i;
    char buf[1024] = {0};
    sprintf( buf, "../workloads/%s/acs_%d.dat", lee_cache->workload->name,
            lee_cache->slab_class );
    FILE * file = fopen( buf, "w" );
    fprintf(file, "%15s%15s\n", "base", "average");
    for ( i = 0; i < MAX_RD; i ++ ) {
        fprintf(file, "%15d%15.4f\n", i, lee_cache->ave_cache_size[i]);
    }
    fclose(file);

    fprintf( stderr, "[LOG] printed 'acs' to %s\n", buf );

}

void print_reuse_distance_histogram( LLeeCache_t * lee_cache )
{
    /**
     * TODO: only output the first 10001 terms for the time being.
     */
    int i;
    char buf[1024] = {0};
    sprintf( buf, "../workloads/%s/rd+_%d.dat", lee_cache->workload->name, 
            lee_cache->slab_class );
    FILE * file = fopen( buf, "w" );
    fprintf(file, "%15s%15s\n", "rd", "count");
    for ( i = 0; i < MAX_RD; i ++ ) {
        fprintf(file, "%15d%15d\n", i, lee_cache->rd_hist[i]);
    }
    fclose(file);

    fprintf( stderr, "[LOG] printed 'rd+' to %s\n", buf );
}

void print_lee_cache_mrc( LLeeCache_t * lee_cache )
{
    /**
     * compute miss ratio curve -
     */
    double *mr;
    mr = (double *)malloc(sizeof(double) * (MAX_NUM_ITEM + 1));
    memset( mr, 0, sizeof(double) * (MAX_NUM_ITEM + 1) );

    int32_t *rd_hist_temp = (int32_t *)malloc( sizeof(int32_t) * (MAX_NUM_ITEM + 1) );
    memcpy( rd_hist_temp, lee_cache->rd_hist, sizeof(int32_t) * (MAX_NUM_ITEM + 1) );

    int64_t N = rd_hist_temp[0] + rd_hist_temp[1];
    int i;
    for ( i = 2; i <= MAX_NUM_ITEM; i ++ ) {
        N += rd_hist_temp[i];
        rd_hist_temp[i] += rd_hist_temp[i-1];
    }
    mr[0] = 1.0;
    for ( i = 1; i <= MAX_NUM_ITEM; i ++ ) {
        mr[i] = 1 - ( (double)rd_hist_temp[i] ) / N;
    }

    free( rd_hist_temp );

    /* print it out. */
    /**
     * TODO: only output the first 10001 terms for the time being.
     */
    char buf[1024] = {0};
    sprintf( buf, "../workloads/%s/result/mrc_%d.dat", lee_cache->workload->name, 
            lee_cache->slab_class );
    FILE * file = fopen( buf, "w" );
    fprintf(file, "%15s%15s%15s\n", "base", "average", "mr");
    for ( i = 0; i < MAX_RD; i ++ ) {
        fprintf(file, "%15d%15.5f%15.6f\n", i, lee_cache->ave_cache_size[i], mr[i]);
    }
    fclose( file );
    free( mr );

    fprintf( stderr, "[LOG] printed 'mrc' to %s\n", buf );
}

void print_traffic_with_central_storage( LLeeCache_t * lee_cache )
{
    fprintf( stderr, "[LOG] communications: %15lld\n", lee_cache->central_storage.communications );
    fprintf( stderr, "[LOG]        fetches: %15lld\n", lee_cache->central_storage.fetches );
    fprintf( stderr, "[LOG]   fetch amount: %15lld\n", lee_cache->central_storage.fetch_amount );
    fprintf( stderr, "[LOG]        returns: %15lld\n", lee_cache->central_storage.returns );
    fprintf( stderr, "[LOG]  return amount: %15lld\n", lee_cache->central_storage.return_amount );
}

void lee_cache_stats( LLeeCache_t * lee_cache )
{
    fprintf( stderr, "[LOG]     slab class: %15d\n",   lee_cache->slab_class );
    fprintf( stderr, "[LOG]       accesses: %15lld\n", lee_cache->stats.accesses );
    fprintf( stderr, "[LOG]           hits: %15lld\n", lee_cache->stats.hits );
    fprintf( stderr, "[LOG]         misses: %15lld\n", lee_cache->stats.misses );

    print_traffic_with_central_storage( lee_cache );

#if defined(AVERAGE_CACHE_SIZE_PROFILING)
    compute_average_cache_size( lee_cache->live_objs_hist, lee_cache->ave_cache_size );
    /* print_average_cache_size( lee_cache ); */
#endif
#if defined(REUSE_DISTANCE_PROFILING)
    /* print_reuse_distance_histogram( lee_cache ); */
#endif
#if defined(MRC_PROFILING)
    print_lee_cache_mrc( lee_cache );
#endif
}
#endif

void lee_cache_op_entrance ( LLeeCache_t * lee_cache, int64_t key_id )
{
    LItem_t * item = search_item_in_lee_cache ( lee_cache, key_id );

#if defined(CACHE_STATS)
    lee_cache->stats.accesses ++;
#endif

    /* not found */
    if ( item == NULL ) {

#if defined(CACHE_STATS)
        lee_cache->stats.misses ++;
        totalMisses++;
#endif
#if defined(REUSE_DISTANCE_PROFILING)
        lee_cache->rd_hist[0] ++;
#endif

        int32_t lease_time = search_lease_time_for_key( key_id );

/* #if defined( */
        /* modify lease time */
        /* if (global_timer >= TRACE_LENGTH_THRESHOLD) { */
        /*     if (lease_time == 0 && accesses[key_id] != 0) { */
        /*         key_lease_map[key_id] = local_timers[lee_cache->slab_class] - accesses[key_id]; */
        /*         lease_time = key_lease_map[key_id]; */
        /*     } */
        /* } */


        if ( lease_time != 0 ) {
            if ( is_lru_component_empty ( lee_cache ) ) {
                /* fetch memory from central storage. */
                lee_cache->central_storage.communications ++;
                lee_cache->central_storage.fetches ++;
                lee_cache->central_storage.fetch_amount += lee_cache->grow_factor;
        
                /* add one more item */
                item = alloc_item( key_id, lease_time, NONE );

                /* build hash relation of this item */
                link_to_hash_table( lee_cache, item );
                
                /* hang it onto lease cache */
                add_to_lease_cache( lee_cache->lease_comp, item );

                /* increment lee_cache size */
                lee_cache->size += 1;

                /* update this item's last access time. */
                item->last_access_time = local_timers[lee_cache->slab_class];
            }
            else {
                LItem_t * tail_item = tail_of_lru_cache( lee_cache->lru_comp );

                /* 1. remove it from lru component. */
                remove_from_lru_cache( lee_cache->lru_comp, tail_item );

                /* 2. replace with new item. */
                update_item( tail_item, key_id, lease_time, NONE );

                /* 3. re-link to hash table. */
                relink_to_hash_table( lee_cache, tail_item );

                /* 4. add it to lease component. */
                add_to_lease_cache( lee_cache->lease_comp, tail_item );
    
                /* 5. update this item's last access time. */
                tail_item->last_access_time = local_timers[lee_cache->slab_class];
            }
        }
        else {
            if ( !is_lru_component_empty ( lee_cache ) ) {
                /* 1. evict tail item. */
                LItem_t * tail_item = tail_of_lru_cache( lee_cache->lru_comp );

                /* 2. replace with new item. */
                update_item( tail_item, key_id, 0, LRU );

                /* 3. re-link to hash table. */
                relink_to_hash_table( lee_cache, tail_item );

                /* 4. LRU update - move from tail to head. */
                update_in_lru_cache( lee_cache->lru_comp, tail_item );

                /* 5. update this item's last access time. */
                tail_item->last_access_time = local_timers[lee_cache->slab_class];
            }
            else {
                /**
                 * Branching for code comments -  
                 *   Because current cache size is beyond base size, and the item's 
                 *   lease time is zero, we do not store the item in the in-memory 
                 *   cache. 
                 */
            }
        }
    }
    else {

#if defined(CACHE_STATS)
        lee_cache->stats.hits ++;
        totalHits++;
#endif

        if ( is_item_in_lru(item) ) {
#if defined(REUSE_DISTANCE_PROFILING)
            int32_t rd = lee_cache->lease_comp->size;
            LItem_t * tmp;
            list_for_each_entry(tmp, &lee_cache->lru_comp->the_cache, list) {
                rd ++;
                if ( tmp->key_id == item->key_id ) break;
            }
            if ( rd <= MAX_NUM_ITEM ) {
                lee_cache->rd_hist[rd] ++;
            }
            else {
                fprintf(stderr, "[WARN] reuse distance greater than MAX_NUM_TEIM. ");
            }
#endif
            if ( item->lease_time != 0 ) {
                /* 1. remove it from lru component. */
                remove_from_lru_cache( lee_cache->lru_comp, item );

                /* 2. add it to lease component. */
                add_to_lease_cache( lee_cache->lease_comp, item );
            }
            else {
                /* LRU update - move to head */
                update_in_lru_cache( lee_cache->lru_comp, item );
            }
        }
        else if ( is_item_in_lease(item) ) {
#if defined(REUSE_DISTANCE_PROFILING)
            /**
             * when found in lease cache, its reuse distance is zero.
             */
            lee_cache->rd_hist[1] ++;
#endif
            if ( item->lease_time != 0 ) {
                /* update to a new position in lease cache. */
                update_in_lease_cache( lee_cache->lease_comp, item );
            }
            else {
                /**
                 * support dynamic lease change, but seldomly happend - 
                 *   item's lease time changes to 0. 
                 */
                /* 1. remove it from lease componenet. */
                remove_from_lease_cache( lee_cache->lease_comp, item ); 

                /* 2. add it to lru head. */
                add_to_lru_cache( lee_cache->lru_comp, item );
            }
        }
        else{
            printf("[ERR] item neither in LEASE nor in LRU.\n");
            assert(0);
        }

        /* update this item's last access time. */
        item->last_access_time = local_timers[lee_cache->slab_class];
    }

    /* update key's access time */
    accesses[key_id] = local_timers[lee_cache->slab_class];

#if defined(AVERAGE_CACHE_SIZE_PROFILING)
    int32_t num_live_objs = lee_cache->lease_comp->size;
    lee_cache->live_objs_hist[num_live_objs] ++;
#endif

    /* check expiration - do with zero-th bin of lease cache. */
    offload_timeout_bin_of_lease_cache( lee_cache->lease_comp, lee_cache->lru_comp );

    /* check and return memory to central storage. */
    if ( lee_cache->size > lee_cache->base_size ) {

        int32_t diff = lee_cache->size - lee_cache->base_size;
        if ( !is_lru_component_empty( lee_cache ) ) {

            lee_cache->central_storage.communications ++;
            lee_cache->central_storage.returns ++;

            int32_t i = diff;
            while ( i && !is_lru_component_empty( lee_cache ) ) {

                /* 1. get the tail item. */
                LItem_t * tail_item = tail_of_lru_cache( lee_cache->lru_comp );

                /* 2. remove from lru cache. */
                remove_from_lru_cache( lee_cache->lru_comp, tail_item );

                /* 3. destroy it. */
                delete_item( tail_item );

                /* 4. counter decrements. */
                -- i;
            }
            lee_cache->central_storage.return_amount += ( diff - i );
            lee_cache->size -= ( diff - i );
        }
    }

    /* sanity check */
    assert( lee_cache->size >= lee_cache->base_size );
    assert( lee_cache->size == lee_cache->lru_comp->size 
            + lee_cache->lease_comp->size );

    return;
}

#endif /* _L_LEASE_LRU_H_ */
