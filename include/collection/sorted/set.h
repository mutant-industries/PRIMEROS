/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Sorted set implemented as doubly-linked circular list
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_COLLECTION_SORTED_SET_H_
#define _SYS_COLLECTION_SORTED_SET_H_

#include <stdbool.h>
#include <collection/deque.h>

// -------------------------------------------------------------------------------------

#define sorted_set_item(item) ((Sorted_set_item_t *) (item))
#define sorted_set(container) ((Sorted_set_item_t **) (container))

// getter, setter
#define sorted_set_item_priority(item) sorted_set_item(item)->_priority

// -------------------------------------------------------------------------------------

/**
 * Sorted set item - deque item with priority
 */
typedef struct Sorted_set_item {
    // resource, chainable
    Deque_item_t _chainable;
    // item priority the set is sorted by
    priority_t _priority;

} Sorted_set_item_t;

/**
 * Place item in given set, order by priority (desc)
 *  - return true if item has highest priority in given set
 */
bool sorted_set_add(Sorted_set_item_t **set, Sorted_set_item_t *item);

/**
 * Remove and return item with highest priority, return NULL if set is empty
 */
Sorted_set_item_t *sorted_set_poll_last(Sorted_set_item_t **set);

/**
 * Set item priority and preserve sorting of set it is (possibly) linked to
 *  - assume that item is linked to round-robin deque ordered by item priority in descending order
 *  - if priority equals actual item priority, make it 'first' of all items with the same priority
 *    -> 'first' when set sorting is understood as if priority grows from first item to last (internal sorting is descending)
 *  - return true if item belongs to some set and has highest priority in that set
 */
bool sorted_set_item_set_priority(Sorted_set_item_t *item, priority_t priority);


#endif /* _SYS_COLLECTION_SORTED_SET_H_ */
