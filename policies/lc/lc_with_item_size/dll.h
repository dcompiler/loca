#ifndef _DLL_H_
#define _DLL_H_

#include "data_structures.h"
#include "declarations.h"
#include "item.h"

/* ---- double linked list implementation ---- */

/* get the head of a linked list */
LItem_t * DLLHead( struct list_head * head )
{
    LItem_t * entry;
    entry = list_first_entry(head, LItem_t, list);
    
    return entry;
}

/* get the tail of a linked list */
LItem_t * DLLTail( struct list_head * head )
{
    LItem_t * entry;
    entry = list_entry(head->prev, LItem_t, list);
    
    return entry;
}

/* move the node "list" to the head of the linked list "head" */
void DLLUpdateHead( struct list_head * head, struct list_head * list )
{
    list_del(list);
    list_add(list, head);
}

/* move the node "list" to the tail of the linked list "head" */
void DLLMoveToTail( struct list_head * head, struct list_head * list )
{
    list_move_tail(list, head);
}

void DLLRemove( struct list_head * list )
{
    list_del(list);
}

void DLLAdd( struct list_head * head, struct list_head * list )
{
    list_add(list, head);
}

int DLLEmpty( struct list_head * head ) 
{
    return list_empty(head);
}

#endif /* _DLL_H_ */
