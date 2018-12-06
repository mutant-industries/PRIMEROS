/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Priority queue implemented as doubly-linked circular list, interrupt-safe
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_COLLECTION_SORTED_QUEUE_H_
#define _SYS_COLLECTION_SORTED_QUEUE_H_

#include <stdbool.h>
#include <collection/deque.h>

// -------------------------------------------------------------------------------------

#define sorted_queue_item(item) ((Sorted_queue_item_t *) (item))
#define sorted_queue(container) ((Sorted_queue_item_t **) (container))

/**
 * Sorted queue public API access
 */
#define enqueue(item, container) sorted_queue_enqueue(sorted_queue_item(item), sorted_queue(container))
#define pop(container) sorted_queue_pop(sorted_queue(container))
#define set_priority(item, priority) sorted_queue_set_priority(sorted_queue_item(item), priority)

// getter, setter
#define sorted_queue_item_priority(item) ((sorted_queue_item(item))->_priority)

// -------------------------------------------------------------------------------------

/**
 * Sorted (priority) queue item
 */
typedef struct Sorted_queue_item {
    // resource, chainable
    Deque_item_t _chainable;
    // item priority the queue is sorted by
    priority_t _priority;

} Sorted_queue_item_t;

/**
 * Place item in given queue, order by priority (desc)
 *  - return true if item has highest priority on given queue
 */
bool sorted_queue_enqueue(Sorted_queue_item_t *item, Sorted_queue_item_t **queue);

/**
 * Remove and return item with highest priority, return NULL if queue empty
 */
Sorted_queue_item_t *sorted_queue_pop(Sorted_queue_item_t **queue);

/**
 * Set item priority and preserve sorting of queue it is (possibly) linked to
 *  - assume that item is linked to round-robin priority queue ordered by item priority in descending order
 *  - if priority is same as actual item priority, make it last of all items with the same priority
 *  - return true if item has highest priority on given queue
 *  or that it is not linked to any queue
 */
bool sorted_queue_set_priority(Sorted_queue_item_t *item, priority_t priority);


#endif /* _SYS_COLLECTION_SORTED_QUEUE_H_ */
