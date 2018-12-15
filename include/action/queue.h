/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Action queue - sorted / FIFO, thread-safe
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_ACTION_QUEUE_H_
#define _SYS_ACTION_QUEUE_H_

#include <stdbool.h>
#include <stddef.h>
#include <action.h>
#include <defs.h>


// -------------------------------------------------------------------------------------

#define action_queue(_queue) ((Action_queue_t *) (_queue))

/**
 * Action queue public API
 */
#define action_queue_create(...) _ACTION_QUEUE_CREATE_GET_MACRO(__VA_ARGS__, __aqc_6, __aqc_5, __aqc_4, __aqc_3)(__VA_ARGS__)
#define action_queue_insert(_queue, _action) (_queue)->insert(_queue, action(_action))
#define action_queue_pop(_queue) (_queue)->pop(_queue)
#define action_queue_trigger_all(_queue, _signal) (_queue)->trigger_all((_queue), signal(_signal))
#define action_queue_close(_queue, _signal) (_queue)->close((_queue), signal(_signal))

//<editor-fold desc="variable-args - action_queue_create()">
#define _ACTION_QUEUE_CREATE_GET_MACRO(_1,_2,_3,_4,_5,_6,NAME,...) NAME
#define __aqc_3(_queue, _sorted, _strict_sorting) \
    action_queue_init(action_queue(_queue), _sorted, _strict_sorting, NULL, NULL, NULL)
#define __aqc_5(_queue, _sorted, _strict_sorting, _owner, _on_head_priority_changed) \
    action_queue_init(action_queue(_queue), _sorted, _strict_sorting, _owner, ((head_priority_changed_hook_t) (_on_head_priority_changed)), NULL)
#define __aqc_6(_queue, _sorted, _strict_sorting, _owner, _on_head_priority_changed, _on_released) \
    action_queue_init(action_queue(_queue), _sorted, _strict_sorting, _owner, ((head_priority_changed_hook_t) (_on_head_priority_changed)), ((action_released_hook_t) (_on_released)))

//</editor-fold>

// getter, setter
#define action_queue_head(_queue) (_queue)->_head
#define action_queue_is_empty(_queue) ( ! action_queue_head(_queue))
#define action_queue_is_closed(_queue) ((_queue)->insert == (bool (*)(Action_queue_t *, Action_t *)) unsupported_after_disposed)
#define action_queue_get_head_priority(_queue) (_queue)->_head_priority
#define action_queue_on_action_released(_queue) (_queue)->_on_action_released
#define action_queue_on_head_priority_changed(_queue) (_queue)->_on_head_priority_changed

// -------------------------------------------------------------------------------------

typedef struct Action_queue Action_queue_t;

/**
 * Action queue 'on released' hook interface
 */
typedef void (*action_released_hook_t)(void *owner, Action_t *, Action_queue_t *origin);

/**
 * Action queue 'on head priority changed' hook interface
 *  - interface compatible with {@see action_default_set_priority}, typical usage - owner inherits queue head priority
 *  - applies only for sorted queue
 */
typedef void (*head_priority_changed_hook_t)(void *owner, priority_t, Action_queue_t *origin);

/**
 * Action queue, queue manipulation interface
 */
struct Action_queue {
    // action with highest priority in queue if sorted, first inserted if FIFO
    Action_t *_head;
    // owner passed to following hooks
    void *_owner;
    // hook triggered after action is removed from queue
    action_released_hook_t _on_action_released;
    // hook triggered when priority of queue head changes, only applies for sorted queue
    head_priority_changed_hook_t _on_head_priority_changed;

    // -------- state --------
    // priority of item with highest priority, only applies for sorted queue
    priority_t _head_priority;
    // iterator state for thread-safe trigger_all
    Action_t *_iterator;

    // -------- package-protected (action) --------
    void (*_release)(Action_t *);
    bool (*_set_action_priority)(Action_t *, priority_t, Action_queue_t *_this);

    // -------- public --------
    bool (*insert)(Action_queue_t *_this, Action_t *);
    Action_t *(*pop)(Action_queue_t *_this);
    void (*trigger_all)(Action_queue_t *_this, signal_t);
    void (*close)(Action_queue_t *_this, signal_t);

};

 /**
  * Initialize action queue
  *
  * @param sorted
  *  - when set, queue acts as priority queue and pop() always returns action with highest priority
  *  - otherwise, queue is simple FIFO (first in first out)
  * @param strict_sorting determines behavior of action_set_priority() when trigger_all() is running
  *  - only applies if 'sorted' is set
  *  - if set, queue sorting rules are not changed, which might result in following:
  *   - if priority is increased or set to the same value, action might not be executed at all
  *   - if priority is decreased, action might be executed twice or not at all
  *   -> this option is useful in cases where strict sorting is a must and trigger_all() never runs,
  * such as runnable process queue, or with queue with actions whose priority is only changed, when trigger_all() is not running
  *  - if not set, when trigger_all() is running, priority of action is just changed without reordering, then:
  *   - action is always executed exactly once during trigger_all()
  *   - queue might no longer be sorted
  *   -> this approach is useful when order of execution does not matter that much, such as event subscription list,
  * this also makes perfect sense, when changing priority of action, that shall remove itself from queue on trigger
  * @param owner optional owner passed to following hooks
  * @param on_head_priority_changed
  *  - optional callback, only applies if 'sorted' is set, interrupts are disabled during execution, triggered when:
  *   - new action with higher priority than current queue head is inserted
  *   - actual queue head is removed, new queue head has lower priority that previous or queue becomes empty
  *   - priority of current queue head is increased
  *   - priority of current queue head is decreased and another action with lower priority becomes new queue head
  *  - hook interface is compatible with action_default_set_priority() and if hook is set to this function,
  * then queue owner inherits priority of queue head
  *  - within hook execution it is only allowed to change priority of single action, {@see action_default_set_priority}
  * @param on_released
  *  - optional callback, executed after action is removed from given queue, interrupts are disabled during execution
  */
void action_queue_init(Action_queue_t *queue, bool sorted, bool strict_sorting, void *owner,
        head_priority_changed_hook_t on_head_priority_changed, action_released_hook_t on_released);

// -------------------------------------------------------------------------------------


#endif /* _SYS_ACTION_QUEUE_H_ */
