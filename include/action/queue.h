/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Action queue, interrupt-safe
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_ACTION_QUEUE_H_
#define _SYS_ACTION_QUEUE_H_

#include <stdbool.h>
#include <defs.h>
#include <action.h>


// -------------------------------------------------------------------------------------

#define action_queue(_queue) ((Action_queue_t *) (_queue))

/**
 * Action queue public API
 */
#define action_queue_empty(_queue) ( ! action_queue_head(_queue))

// getter, setter
#define action_queue_head(_queue) (action_queue(_queue)->_head)

// -------------------------------------------------------------------------------------

/**
 * Action queue state with reference to queue head
 */
typedef struct Action_queue {
    // queue head - element with highest priority in given queue
    Action_t *_head;
    // iterator state for thread-safe trigger_all
    Action_t *_iterator;
    // behavior of action_set_priority() when trigger_all() is running
    bool _strict_sorting;

} Action_queue_t;

/**
 * Initialize given queue
 *  - action queue is not a resource and it is responsibility of queue owner to remove all actions before queue is released
 *  - strict_sorting determines behavior of action_set_priority() when trigger_all() is running
 *    - if set, queue sorting rules are not changed, which might result in following:
 *      - if priority is increased or set to the same value, action might not be executed at all
 *      - if priority is decreased, action might be executed twice or not at all
 *      -> this option is useful in cases where strict sorting is a must and trigger_all() never runs,
 * such as runnable process queue, or with some queue, which does not contain actions with dynamic priority or whose priority
 * is only changed, when trigger_all() is not running
 *    - if not set, when trigger_all() is running, priority of action is just changed without reordering, then:
 *      - action is always executed exactly once during trigger_all()
 *      - queue might no longer be sorted
 *      -> this approach is useful when order of execution does not matter that much, such as event subscription list,
 * this also makes perfect sense, when changing priority of action, that shall remove itself from queue on trigger
 */
void action_queue_init(Action_queue_t *queue, bool strict_sorting);

// -------------------------------------------------------------------------------------

/**
 * Trigger all actions in given queue with given signal (starting from action with highest priority)
 *  - action removal (or pop) from given queue during trigger_all() is completely thread-safe, however once queue iterator reaches
 *  some action, it is going to be executed, no matter if it is removed from queue during execution
 *  - adding actions to given queue during trigger_all() is also thread-safe, but depending on queue content and priority
 *  of added action and also on current state of queue iterator - new action might not be executed
 *  - changing priority of action in given queue during trigger_all() is also thread safe, {@see action_queue_init}
 *  - trigger_all() has no protection against concurrent execution from different contexts, this must be handled elsewhere by mutex
 */
void action_queue_trigger_all(Action_queue_t *queue, signal_t signal);

/**
 * Remove and return action with highest priority, return NULL if queue is empty
 *  - if trigger_all() is running, adjust iterator state
 */
Action_t *action_queue_pop(Action_queue_t *queue);

/**
 * Remove action from whatever queue it is linked to, do nothing if action is not linked to any queue
 *  - if trigger_all() on queue given action is linked to is running, adjust iterator state
 */
void action_remove(Action_t *action);

/**
 * Change priority of given action, preserve queue sorting
 *  - for behavior when trigger_all() is running {@see action_queue_init}
 *  - return true if action has highest priority on given queue
 */
bool action_set_priority(Action_t *action, priority_t priority);


#endif /* _SYS_ACTION_QUEUE_H_ */
