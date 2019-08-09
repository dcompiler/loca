#ifndef _L_ITEM_H_
#define _L_ITEM_H_

typedef enum {NONE, LEASE, LRU} POSITION;

struct _LItem_t {
    struct   list_head list;
    int64_t  key_id;
    int64_t  value;
    int32_t  lease_time;
    int64_t  last_access_time;
    POSITION lease_or_lru;
    HashTableElem_t  * hash_node; /* for an easy traversal. */
} ATTR_ALIGN_TO_AVOID_FALSE_SHARING;

void print_item( struct _LItem_t * item )
{
    printf("[LOG]           key id: %lld\n", item->key_id);
    printf("[LOG]       lease time: %d\n", item->lease_time);
    printf("[LOG] last access time: %lld\n", item->last_access_time);
    printf("[LOG]  on lease or lru: %d\n", item->lease_or_lru);
    printf("[LOG]  address of list: %p\n", &item->list);
}

#endif
