/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Doubly-linked circular list
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_COLLECTION_LIST_H_
#define _SYS_COLLECTION_LIST_H_

#include <resource.h>

// -------------------------------------------------------------------------------------

/**
 * Disposable list item, that allows bi-directional chaining
 *  - no list-related dispose handler, list item is never removed from list automatically
 */
typedef struct List_item {
    // enable dispose(List_item_t *)
    Disposable_t _disposable;
    // enable bi-directional chaining
    struct List_item *_next;
    struct List_item *_prev;

} List_item_t;

// -------------------------------------------------------------------------------------

/**
 * Add item to end of given list, allow bidirectional (round-robin) list traversal
 *  - no check whether the item already belongs to given list
 */
void list_add_last(List_item_t **list, List_item_t *item);

/**
 * Remove item from given list
 *  - no check whether the item actually belongs to given list
 */
void list_remove(List_item_t **list, List_item_t *item);


#endif /* _SYS_COLLECTION_LIST_H_ */
