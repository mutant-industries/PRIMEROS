// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <collection/deque.h>
#include <stdbool.h>
#include <stddef.h>


void deque_insert_first(Deque_item_t **container, Deque_item_t *item) {

    deque_insert_last(container, item);
    // just make tail new head
    *container = item;
}

void deque_insert_last(Deque_item_t **container, Deque_item_t *item) {

    if (item->_container) {
        deque_item_remove(item);
    }

    if ( ! *container) {
        *container = item;
        item->_next = item;
        item->_prev = item;
    }
    else {
        // place item to the end of deque
        item->_next = *container;
        item->_prev = (*container)->_prev;
        (*container)->_prev->_next = item;
        (*container)->_prev = item;
    }

    item->_container = container;
}

void deque_insert_after(Deque_item_t *item, Deque_item_t *after_item) {

    // sanity check
    if ( ! after_item->_container || item == after_item) {
        return;
    }

    if (item->_container) {
        deque_item_remove(item);
    }

    item->_next = after_item->_next;
    item->_prev = after_item;

    after_item->_next->_prev = item;
    after_item->_next = item;

    item->_container = after_item->_container;
}

void deque_insert_before(Deque_item_t *item, Deque_item_t *before_item) {

    // sanity check
    if ( ! before_item->_container) {
        return;
    }

    deque_insert_after(item, before_item->_prev);

    if (*(before_item->_container) == before_item) {
        *(before_item->_container) = item;
    }
}

// -------------------------------------------------------------------------------------

void deque_item_remove(Deque_item_t *item) {

    if ( ! item->_container) {
        // item does not belong to any container, nothing to be done
    }
    else if (*item->_container == item && item->_next == item) {
        // last item in container
        *item->_container = NULL;
    }
    else {
        if (item->_prev) {
            item->_prev->_next = item->_next;
        }

        if (item->_next) {
            item->_next->_prev = item->_prev;
        }

        if (*item->_container == item) {
            *item->_container = item->_next;
        }
    }

    // discard all references to the container items
    item->_next = item->_prev = NULL;
    // discard reference to container
    item->_container = NULL;
}
