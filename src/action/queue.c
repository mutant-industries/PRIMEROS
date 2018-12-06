// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action/queue.h>
#include <stddef.h>
#include <driver/interrupt.h>


// move queue iterator to next position after current or set NULL when current->next is queue head
#define _iterator_advance(_queue) (_queue)->_iterator = action(deque_item_next((_queue)->_iterator)) == (_queue)->_head ? \
                    NULL : action(deque_item_next((_queue)->_iterator));

// -------------------------------------------------------------------------------------

// Action_queue_t constructor
void action_queue_init(Action_queue_t *queue, bool strict_sorting) {
    queue->_head = NULL;
    queue->_strict_sorting = strict_sorting;
}

// -------------------------------------------------------------------------------------

void action_queue_trigger_all(Action_queue_t *queue, signal_t signal) {
    Action_t *current;

    interrupt_suspend();

    // current queue head is now going to be triggered if set
    current = queue->_iterator = queue->_head;

    // prepare queue iterator if queue not empty
    if (current) {
        _iterator_advance(queue);
    }

    interrupt_restore();

    while (current) {
        // execute action with given signal
        action_trigger(current, signal);

        interrupt_suspend();

        // move to next action in queue, current is now going to be triggered if set
        current = queue->_iterator;

        // set iterator to current->next or to NULL if current->next is queue head
        if (current) {
            _iterator_advance(queue);
        }

        interrupt_restore();
    }
}

// -------------------------------------------------------------------------------------

Action_t *action_queue_pop(Action_queue_t *queue) {
    Action_t *head;

    interrupt_suspend();

    if (queue->_head && queue->_iterator == queue->_head) {
        _iterator_advance(queue);
    }

    head = action(pop(queue));

    interrupt_restore();

    return head;
}

void action_remove(Action_t *action) {
    Action_queue_t *queue;

    interrupt_suspend();

    if ((queue = action_queue(deque_item_container(action)))) {
        if (queue->_iterator == action) {
            _iterator_advance(queue);
        }

        remove(action);
    }


    interrupt_restore();

}

bool action_set_priority(Action_t *action, priority_t priority) {
    bool highest_priority_placement = false;
    Action_queue_t *queue;

    interrupt_suspend();

    if ((queue = action_queue(deque_item_container(action))) && queue->_iterator) {
        // action_queue_trigger_all() running
        if (queue->_strict_sorting) {
            // strict sorting - no changes in sorting behavior
            if (queue->_iterator == action) {
                // just advance iterator as if action was removed from queue
                _iterator_advance(queue);
            }

            highest_priority_placement = set_priority(action, priority);
        }
        else {
            // just update action priority without queue re-sorting
            sorted_queue_item_priority(action) = priority;
        }
    }
    else {
        // standard behavior
        highest_priority_placement = set_priority(action, priority);
    }

    interrupt_restore();

    return highest_priority_placement;
}
