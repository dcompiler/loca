/**
  *    -- c++ --
  *
  *    By Pengcheng Li <pli@cs.rochester.edu>
  */

#ifndef _L_LRU_CACHE_H_
#define _L_LRU_CACHE_H_

#include "hash.h" /* doubly linked list, stack and hash table */
#include "dll.h"
#include "attrs.h"
#include "declarations.h"

/**
  *  Data structure of the software cache.
  */
struct _LLRUCache_t {
    struct list_head the_cache;
    size_t  size;	
    int32_t slab_class;
} ATTR_ALIGN_TO_AVOID_FALSE_SHARING;

/* item does not exist, but added to head now. */
void add_to_lru_cache( LLRUCache_t * lru_cache, LItem_t * item )
{
//    item->lease_or_lru = LRU;
    DLLAdd( &lru_cache->the_cache, &item->list );
    lru_cache->size += 1;
}

/* item exists, move to head now. */
void update_in_lru_cache( LLRUCache_t * lru_cache, LItem_t * item )
{
    /* LRU algorithm: move the found node to the head */
    DLLUpdateHead( &lru_cache->the_cache, &item->list );

    /* no need for the change of hash table. */
}

/* item exists, remove it from cache. */
void remove_from_lru_cache( LLRUCache_t * lru_cache, LItem_t * item )
{
    DLLRemove( &item->list );
    lru_cache->size -= 1;
}

LItem_t * tail_of_lru_cache( LLRUCache_t * lru_cache )
{
    return DLLTail( &lru_cache->the_cache );
}

LLRUCache_t * create_lru_cache( size_t size, int32_t slab_class)
{
    LLRUCache_t * lru_cache = (LLRUCache_t *)malloc(sizeof(LLRUCache_t));
    memset( lru_cache, 0, sizeof(LLRUCache_t) );

    /* create the software cache */
    INIT_LIST_HEAD(&lru_cache->the_cache);

    lru_cache->size = size;
    lru_cache->slab_class = slab_class;

    return lru_cache;
}

void print_lru_cache( LLRUCache_t * lru_cache );
/* destroy the software cache */
void destroy_lru_cache( LLRUCache_t * lru_cache )
{
    LItem_t * tmp;
    LItem_t * n;
    HashTableElem_t * m;
    list_for_each_entry_safe(tmp, n, &lru_cache->the_cache, list) {
        m = tmp->hash_node;
        hlist_del(&m->list);
        free(m);
        list_del(&tmp->list);
        free(tmp);
    }
}

int32_t lru_cache_empty( LLRUCache_t * lru_cache )
{
    return DLLEmpty( &lru_cache->the_cache );
}

void print_lru_cache( LLRUCache_t * lru_cache )
{
    LItem_t * tmp;
    list_for_each_entry(tmp, &lru_cache->the_cache, list) {
        print_item( tmp );
    }
}

/* export interface to report cache misses */
void stats_of_lru_cache( LLRUCache_t * lru_cache )
{
}

#endif /* _L_LRU_CACHE_H_ */
