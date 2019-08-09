#ifndef _L_ORDERED_LEASE_CACHE_H_
#define _L_ORDERED_LEASE_CACHE_H_

#include "hash.h" /* doubly linked list, stack and hash table */
#include "dll.h"
#include "attrs.h"
#include "declarations.h"

struct _LOrderedLeaseCache_t {
    size_t  size;
    int32_t max_lease_time;
    int32_t tail; /* points to timeout bin */
    struct  list_head * bins;
    int32_t *bin_size;

    int32_t slab_class;
};

LOrderedLeaseCache_t * create_ordered_lease_cache( size_t size, 
        int32_t max_lease_time, int32_t slab_class)
{
    LOrderedLeaseCache_t * lease_cache = 
        (LOrderedLeaseCache_t *)malloc(sizeof(LOrderedLeaseCache_t));
    memset( lease_cache, 0, sizeof(LOrderedLeaseCache_t) );

    lease_cache->size = size;
    lease_cache->max_lease_time = max_lease_time;
    lease_cache->slab_class = slab_class;
    lease_cache->bins = 
        (struct list_head *)malloc(sizeof(struct list_head) * (max_lease_time+1));
    memset(lease_cache->bins, 0, sizeof(struct list_head) * (max_lease_time+1));
    int32_t i;
    for ( i =0; i<=max_lease_time; i++ ) {
        INIT_LIST_HEAD(&lease_cache->bins[i]);
    }
    lease_cache->bin_size = (int32_t *)malloc(sizeof(int32_t) * (max_lease_time+1));
    memset( lease_cache->bin_size, 0, sizeof(int32_t) * (max_lease_time+1) );

    lease_cache->tail = 0;

    return lease_cache;
}

void destroy_ordered_lease_cache( LOrderedLeaseCache_t * lease_cache ) 
{
    /* free each bin of bins. */ 
    int32_t i;
    for ( i=0; i<=lease_cache->max_lease_time; i++ ) {
        LItem_t * tmp;
        LItem_t * n;
        HashTableElem_t * m;
        list_for_each_entry_safe(tmp, n, &lease_cache->bins[i], list) {
            m = tmp->hash_node;
            hlist_del(&m->list);
            free(m);
            list_del(&tmp->list);
            free(tmp);
        }
    }

    free(lease_cache->bins);
    free(lease_cache->bin_size);
    free(lease_cache);
}

void add_to_lease_cache( LOrderedLeaseCache_t * lease_cache, LItem_t * item )
{
    /* go to LRU if yes */
    assert ( item->lease_time != 0 && item->lease_time <= lease_cache->max_lease_time );

    item->lease_or_lru = LEASE;

    int slot;
    slot = ( lease_cache->tail + item->lease_time ) % (lease_cache->max_lease_time+1);

    DLLAdd( &lease_cache->bins[slot], &item->list );
    lease_cache->size += 1;
    lease_cache->bin_size[slot] += 1;
}

void update_in_lease_cache( LOrderedLeaseCache_t * lease_cache, LItem_t * item )
{
    DLLRemove( &item->list );

    int32_t prior_slot, passed_time, last_tail;
    passed_time = local_timers[lease_cache->slab_class] - item->last_access_time;
    last_tail = ( lease_cache->tail - passed_time + lease_cache->max_lease_time+1 ) % (lease_cache->max_lease_time+1);
    prior_slot = ( last_tail + item->lease_time ) % (lease_cache->max_lease_time+1); 
    lease_cache->bin_size[prior_slot] -= 1; 

    int slot;
    slot = ( lease_cache->tail + item->lease_time ) % (lease_cache->max_lease_time+1);

    DLLAdd( &lease_cache->bins[slot], &item->list );
    lease_cache->bin_size[slot] += 1;
}

void remove_from_lease_cache( LOrderedLeaseCache_t * lease_cache, LItem_t * item )
{
    DLLRemove( &item->list );

    int32_t prior_slot, passed_time, last_tail;
    passed_time = local_timers[lease_cache->slab_class] - item->last_access_time;
    last_tail = ( lease_cache->tail - passed_time + lease_cache->max_lease_time+1 ) % (lease_cache->max_lease_time+1);
    prior_slot = ( last_tail + item->lease_time ) % (lease_cache->max_lease_time+1); 
    lease_cache->bin_size[prior_slot] -= 1; 
    lease_cache->size -= 1;
}

void offload_timeout_bin_of_lease_cache( LOrderedLeaseCache_t * lease_cache, LLRUCache_t * lru_cache )
{
    struct list_head * timeout_bin_head = &lease_cache->bins[lease_cache->tail];

    if ( DLLEmpty( timeout_bin_head ) ) goto UPDATE_TAIL;

    /* not empty, change lease_or_lru of the offloaded items to LRU */
    LItem_t * tmp;
    list_for_each_entry(tmp, &lease_cache->bins[lease_cache->tail], list) {
        tmp->lease_or_lru = LRU;
    }

    struct list_head * timeout_bin_first_node = timeout_bin_head->next;
    struct list_head * timeout_bin_tail_node = timeout_bin_head->prev; 
    /* clear this bin */
    INIT_LIST_HEAD( timeout_bin_head );

    /* link this bin to LRU cache */
    struct list_head * lru_cache_head = &lru_cache->the_cache;
    struct list_head * lru_cache_first_node = lru_cache_head->next;

    lru_cache_first_node->prev = timeout_bin_tail_node;
    timeout_bin_tail_node->next = lru_cache_first_node;
    lru_cache_head->next = timeout_bin_first_node;
    timeout_bin_first_node->prev = lru_cache_head;

    /* change cache sizes */
    lease_cache->size -= lease_cache->bin_size[lease_cache->tail];
    lru_cache->size += lease_cache->bin_size[lease_cache->tail];
    lease_cache->bin_size[lease_cache->tail] = 0;

UPDATE_TAIL:
    /* update tail */
    lease_cache->tail = ( lease_cache->tail + 1 ) % (lease_cache->max_lease_time+1);
}

#endif /*_L_ORDERED_LEASE_CACHE_H_*/
