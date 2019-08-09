#ifndef _HASH_H_
#define _HASH_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#include "data_structures.h"
#include "attrs.h"
#include "declarations.h"

#define DEFAULT_BUCKET_SIZE                   (15)   /*hash table size */


struct _HashTableElem_t{
    struct   hlist_node list;
    int64_t  key_id;
    LItem_t  *points_to;  /* T: LItem_t * or else */
} ATTR_ALIGN_TO_AVOID_FALSE_SHARING;

struct _HashTable_t {
    struct hlist_head *bucket;
    uint64_t          bucket_size;
    uint64_t          bucket_mask;
} ATTR_ALIGN_TO_AVOID_FALSE_SHARING;

/* the seed of hash key */
uint32_t hash_rd = 0x20150707;

/* calculate hash key, given a key_id */
uint64_t hashkey( HashTable_t * ht , int64_t key_id )
{
    return ((uint64_t) key_id) & ht->bucket_mask ;
}

/* create a hash table */
HashTable_t * create_hash_table()
{
    HashTable_t * hash_table = (HashTable_t *)malloc(sizeof(HashTable_t));
    hash_table->bucket_size = 1 << DEFAULT_BUCKET_SIZE;
    hash_table->bucket_mask = hash_table->bucket_size -1;
    hash_table->bucket = (struct hlist_head *) malloc
        (hash_table->bucket_size * sizeof(struct hlist_head));
    size_t i;
    for ( i = 0; i < hash_table->bucket_size; i++ )
        INIT_HLIST_HEAD(&hash_table->bucket[i]);

    return hash_table;
}

/** 
 * destroy a hash table -
 *   we assume that all hash_nodes in the hash table are freed already.
 */
void destroy_hash_table( HashTable_t * hash_table )
{
    free( hash_table->bucket );
    free( hash_table );
}

/* add an element to a hash table */
void  HashTableAdd( HashTable_t * ht, HashTableElem_t * elem )
{
    uint64_t hash = hashkey(ht, elem->key_id);
    hlist_add_head( &elem->list, &ht->bucket[hash] );
}

/* get an element in a hash table, given a key_id */
HashTableElem_t * HashTableGetElem( HashTable_t * ht, int64_t key_id )
{
    uint64_t hash = hashkey(ht, key_id);
    HashTableElem_t *elem;
    struct hlist_node *n;

    hlist_for_each_entry( elem, n, &ht->bucket[hash], list ) {
        if ( LIKELY(key_id == elem->key_id) ) {
            return elem;
        }
    }

    return NULL;
}

/* delete an element from a hash table, given a key_id */
HashTableElem_t * HashTableDel(HashTable_t * ht, intptr_t key_id )
{
    HashTableElem_t * elem = HashTableGetElem(ht, key_id);
    hlist_del(&elem->list);
    return elem;
}

/**
 * update a key_id entry with new cache line address. need to do :
 * 1. change the cache line address of the entry;
 * 2. get rid of the "key-value" pair from hash table due to change of key;
 * 3. add new hash element to the hash table.
 */
void HashTableUpdate(HashTable_t * ht, intptr_t old_key_id, intptr_t new_key_id )
{
    /* key changes, thus chang to another bucket. */
    HashTableElem_t * elem = HashTableDel( ht, old_key_id );
    
    /* update the new pointed cache line */
    elem->key_id = new_key_id;
    
    /* NOTE: no need to change the pointed link node, because the linke node's address keeps the same*/
    // elem->list_node = new_node;
    
    HashTableAdd(ht, elem);
}

/* get the node in the linked list of the software cache, given a key_id */
LItem_t * HashTableGet( HashTable_t * ht, intptr_t key_id )
{
    uint64_t hash = hashkey(ht, key_id);
    HashTableElem_t *elem;
    struct hlist_node *n;
    
    hlist_for_each_entry( elem, n, &ht->bucket[hash], list ) {
        if ( LIKELY(key_id == elem->key_id) ) {
            return elem->points_to;
        }
    }
    
    return 0; /* 0 works for both LItem_t * and int32_t */
}



#endif /* _HASH_H_ */
