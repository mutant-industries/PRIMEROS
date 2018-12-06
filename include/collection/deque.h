/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Double-ended queue implemented as doubly-linked circular list, interrupt-safe
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_COLLECTION_DEQUE_H_
#define _SYS_COLLECTION_DEQUE_H_

#include <resource.h>

// -------------------------------------------------------------------------------------

#define deque_item(item) ((Deque_item_t *) (item))
#define deque(container) ((Deque_item_t **) (container))

/**
 * Deque public API access
 */
#define insert_first(item, container) deque_insert_first(deque_item(item), deque(container))
#define insert_last(item, container) deque_insert_last(deque_item(item), deque(container))
#define insert_after(item, after_item) deque_insert_after(deque_item(item), deque_item(after_item))
#define insert_before(item, before_item) deque_insert_before(deque_item(item), deque_item(before_item))
#define remove(item) deque_remove(deque_item(item))

// getter, setter
#define deque_item_container(item) ((deque_item(item))->_container)
#define deque_item_next(item) ((deque_item(item))->_next)
#define deque_item_prev(item) ((deque_item(item))->_prev)

// -------------------------------------------------------------------------------------

typedef struct Deque_item Deque_item_t;

/**
 * Disposable deque item
 *  - no deque-related dispose handler, deque item is never removed from deque automatically
 */
struct Deque_item {
    // resource, enable dispose(Deque_item_t *)
    Disposable_t _disposable;
    // bidirectional chaining
    Deque_item_t *_next;
    Deque_item_t *_prev;
    // container the deque item is linked to
    Deque_item_t **_container;

};

// -------------------------------------------------------------------------------------

/**
 * Add item to start of given deque, allow bidirectional (round-robin) deque traversal
 */
void deque_insert_first(Deque_item_t *item, Deque_item_t **container);

/**
 * Add item to end of given deque, allow bidirectional (round-robin) deque traversal
 */
void deque_insert_last(Deque_item_t *item, Deque_item_t **container);

/**
 * Add item to same deque after_item is linked to after after_item, do nothing if after_item is not linked to any deque
 */
void deque_insert_after(Deque_item_t *item, Deque_item_t *after_item);

/**
 * Add item to same deque before_item is linked to before before_item, do nothing if before_item is not linked to any deque
 */
void deque_insert_before(Deque_item_t *item, Deque_item_t *before_item);

/**
 * Remove item from whatever container it is (possibly) linked to
 */
void deque_remove(Deque_item_t *item);


#endif /* _SYS_COLLECTION_DEQUE_H_ */
