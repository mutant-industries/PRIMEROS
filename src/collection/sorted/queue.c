// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <collection/sorted/queue.h>
#include <driver/interrupt.h>


bool sorted_queue_enqueue(Sorted_queue_item_t *item, Sorted_queue_item_t **queue) {
    Sorted_queue_item_t *current;
    bool highest_priority_placement = false;

    interrupt_suspend();

    // remove item from any (possible) queue
    if (deque_item_container(item)) {
        remove(item);
    }

    // priority zero - add to end of queue
    if ( ! item->_priority) {
        insert_last(item, queue);
    }
    // queue empty or highest priority item
    else if ( ! *queue || item->_priority > (*queue)->_priority) {
        // new head of the queue
        insert_first(item, queue);
        // enqueued item has highest priority
        highest_priority_placement = true;
    }
    else {
        current = *queue;

        // place behind the first element with higher or equal priority or place to end of queue
        while (sorted_queue_item_priority(deque_item_next(current)) >= item->_priority
                    && deque_item_next(current) != deque_item(*queue)) {

            current = sorted_queue_item(deque_item_next(current));
        }

        insert_after(item, current);
    }

    // link given queue to item
    deque_item_container(item) = deque(queue);

    interrupt_restore();

    return highest_priority_placement;
}

Sorted_queue_item_t *sorted_queue_pop(Sorted_queue_item_t **queue) {
    Sorted_queue_item_t *item;

    interrupt_suspend();

    if ((item = *queue)) {
        remove(item);
    }

    interrupt_restore();

    return item;
}

bool sorted_queue_set_priority(Sorted_queue_item_t *item, priority_t priority) {
    Sorted_queue_item_t **queue, *head, *tail, *current;

    interrupt_suspend();

    if ( ! deque_item_container(item)) {
        // item in not in any queue, just set new priority
        item->_priority = priority;

        interrupt_restore();
        // item not in any queue
        return false;
    }

    priority_t original_priority = item->_priority;

    queue = sorted_queue(deque_item_container(item));
    head = *queue;
    tail = sorted_queue_item(deque_item_prev(head));

    item->_priority = priority;

    if (item->_priority <= original_priority) {
        current = item;

        // place behind the first element with higher or equal priority or place to end of queue
        while (sorted_queue_item_priority(deque_item_next(current)) >= item->_priority
                    && deque_item_next(current) != deque_item(*queue)) {

            current = sorted_queue_item(deque_item_next(current));
        }

        insert_after(item, current);
    }
    else if (deque_item_prev(item) == deque_item(item)
                || sorted_queue_item_priority(deque_item_prev(item)) > item->_priority) {
        // single item in queue or item on correct position already, nothing to be done
    }
    else if ( ! item->_priority) {
        // priority zero, goes directly to end of list
        insert_after(item, tail);
    }
    else if (item->_priority > head->_priority) {
        // highest priority, place
        insert_before(item, head);
    }
    else {
        sorted_queue_enqueue(item, sorted_queue(deque_item_container(item)));
    }

    interrupt_restore();

    return item == sorted_queue_item(*deque_item_container(item));
}
