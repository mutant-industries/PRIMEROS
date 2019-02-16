/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Action descriptor, action lifecycle
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_ACTION_H_
#define _SYS_ACTION_H_

#include <stdbool.h>
#include <stddef.h>
#include <collection/sorted/set.h>
#include <defs.h>

// -------------------------------------------------------------------------------------

#define action(_action) ((Action_t *) (_action))

/**
 * Action public API access
 */
#define action_create(...) _ACTION_CREATE_GET_MACRO(__VA_ARGS__, _action_create_4, _action_create_3)(__VA_ARGS__)
#define action_trigger(_action, _signal) action(_action)->trigger(action(_action), signal(_signal))
#define action_release(_action) action_default_release(action(_action))
#define action_set_priority(_action, _priority) action_default_set_priority(action(_action), _priority)
#define action_released_callback(_action, _queue) action(_action)->on_released(action(_action), _queue)

//<editor-fold desc="variable-args - action_create()">
#define _ACTION_CREATE_GET_MACRO(_1,_2,_3,_4,NAME,...) NAME
#define _action_create_3(_action, _dispose_hook, _trigger) \
    action_register(action(_action), _dispose_hook, ((action_trigger_t) (_trigger)), NULL)
#define _action_create_4(_action, _dispose_hook, _trigger, _handler) \
    action_register(action(_action), _dispose_hook, ((action_trigger_t) (_trigger)), ((action_handler_t) (_handler)))
//</editor-fold>

// getter, setter
#define action_attr(_action, _attr) action(_action)->_attr
// typical usage of arg_1 - action owner
#define action_owner_attr arg_1
#define action_owner(_action) action_attr(_action, action_owner_attr)
#define action_handler(_action) action_attr(_action, handler)
#define action_on_released(_action) action(_action)->on_released

// -------------------------------------------------------------------------------------

typedef struct Action Action_t;

typedef signal_t action_arg_t;

/**
 * Generic action trigger interface
 *  - trigger is free to manipulate action state or state of signal or call handler with any arguments
 *  - trigger is free to remove action from queue it is linked to and place to another or change action priority
 *  - trigger execution context, which might be interrupt service or other than owner process, is important to consider
 *    - if execution context does not match action owner, then any blocking calls must be avoided
 *    - also if context does not match and resource management is enabled, then no resources should be allocated
 */
typedef void (*action_trigger_t)(Action_t *_this, signal_t signal);

/**
 * Generic action handler interface
 *  - convention is such that first argument is action owner (arg_1) and second is signal passed to trigger,
 * but this is decided in trigger, which might not call handler at all - handler is basically just general purpose
 * function pointer and its actual purpose is always defined by particular trigger
 *  - if action has 'action_default_trigger' then return false to remove action from list it is linked to
 */
typedef bool (*action_handler_t)(action_arg_t, action_arg_t);

/**
 * Action 'on released' hook interface - triggered after action is removed from queue
 *  - interrupts are disabled during execution
 */
typedef void (*action_released_hook_t)(Action_t *_this, Action_queue_t *origin);

/**
 * Action descriptor
 */
struct Action {
    // resource, sorted set item
    Sorted_set_item_t _sortable;
    // function to be called on dispose
    dispose_function_t _dispose_hook;

    // -------- state --------
    action_arg_t arg_1, arg_2;

    // -------- abstract public --------
    action_trigger_t trigger;
    action_handler_t handler;
    action_released_hook_t on_released;

};


void action_register(Action_t *action, dispose_function_t dispose_hook, action_trigger_t trigger, action_handler_t handler);

// -------------------------------------------------------------------------------------

/**
 * Pass action owner (arg_1) and signal to handler
 *  - if handler returns false, action shall be released from queue it is (possibly) linked to
 */
void action_default_trigger(Action_t *_this, signal_t signal);

/**
 * Release action from whatever queue it is linked to, do nothing if action is not linked to any queue
 *  - queue is notified so that it can trigger appropriate hooks
 */
void action_default_release(Action_t *action);

/**
 * Change priority of given action
 *  - queue is notified so that it can trigger appropriate hooks
 *  - recursion-safe - if this call triggers another recursive calls to action_default_set_priority(), which is typical with
 * priority inheritance (change of action priority triggers change of priority of queue owner), then stack size is constant
 *    - calls to action_default_set_priority() within nested call always return false since they only create request
 * to change priority of given action, request itself is processed after nested call returns
 *    - it is only valid to create single priority change request within nested call (within queue hooks)
 *  - for behavior when trigger_all() is running {@see action_queue_init}
 *  - return true if action has highest priority on queue it is linked to and if that queue is sorted
 */
bool action_default_set_priority(Action_t *action, priority_t priority);


#endif /* _SYS_ACTION_H_ */
