// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <collection/sorted/set.h>


bool sorted_set_add(Sorted_set_item_t **set, Sorted_set_item_t *item) {
    Sorted_set_item_t *current;
    bool highest_priority_placement = false;

    // remove item from any (possible) deque
    if (deque_item_container(item)) {
        deque_item_remove(deque_item(item));
    }

    // set is empty or highest priority item
    if ( ! *set || item->_priority > (*set)->_priority) {
        // new head of the deque
        deque_insert_first(deque(set), deque_item(item));
        // added item has highest priority
        highest_priority_placement = true;
    }
    // priority zero or lowest priority item
    else if (sorted_set_item_priority(deque_item_prev(*set)) >= item->_priority) {
        deque_insert_last(deque(set), deque_item(item));
    }
    else {
        current = *set;

        // place behind the first element with higher or equal priority or place to end of deque
        while (sorted_set_item_priority(deque_item_next(current)) >= item->_priority
                    && deque_item_next(current) != deque_item(*set)) {

            current = sorted_set_item(deque_item_next(current));
        }

        deque_insert_after(deque_item(item), deque_item(current));
    }

    return highest_priority_placement;
}

Sorted_set_item_t *sorted_set_poll_last(Sorted_set_item_t **set) {
    Sorted_set_item_t *item;

    if ((item = *set)) {
        deque_item_remove(deque_item(item));
    }

    return item;
}

bool sorted_set_item_set_priority(Sorted_set_item_t *item, priority_t priority) {
    Sorted_set_item_t *head, *tail, *current;
    bool highest_priority_placement;

    if ( ! deque_item_container(item)) {
        // item in not in any deque, just set new priority
        item->_priority = priority;

        // item not in any deque
        return false;
    }

    head = *sorted_set(deque_item_container(item));
    tail = sorted_set_item(deque_item_prev(head));

    if (deque_item_prev(item) == deque_item(item)) {
        // single item in set, nothing to sort
    }
    else if (priority <= sorted_set_item_priority(tail)) {
        // only move item when it is not tail already
        if (item != tail) {
            // priority zero or lowest priority item, goes directly to end of list
            deque_insert_last(deque_item_container(item), deque_item(item));
        }
    }
    else if (priority > head->_priority) {
        // only move item when it is not head already
        if (item != head) {
            // highest priority, place to start of list
            deque_insert_first(deque_item_container(item), deque_item(item));
        }
    }
    else if (priority <= item->_priority) {
        current = item;

        // place behind the first element with higher or equal priority
        while (sorted_set_item_priority(deque_item_next(current)) >= priority
                    && deque_item_next(current) != deque_item(head)) {

            current = sorted_set_item(deque_item_next(current));
        }

        // only move after current if not item itself
        if (item != current) {
            deque_insert_after(deque_item(item), deque_item(current));
        }
    }
    else if (sorted_set_item_priority(deque_item_prev(item)) > priority) {
        // item on correct position already, nothing to be done
    }
    else {
        // set priority now
        item->_priority = priority;
        // then remove and add item to set again
        sorted_set_add(sorted_set(deque_item_container(item)), item);
    }

    item->_priority = priority;

    highest_priority_placement = (item == sorted_set_item(*deque_item_container(item)));

    return highest_priority_placement;
}
