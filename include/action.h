/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Action descriptor, action execution
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_ACTION_H_
#define _SYS_ACTION_H_

#include <stdbool.h>
#include <collection/sorted/queue.h>
#include <defs.h>

// -------------------------------------------------------------------------------------

#define action(_action) ((Action_t *) (_action))

/**
 * Action public API access
 */
#define action_trigger(_action, _signal) (action(_action)->trigger(action(_action), signal(_signal)))

// -------------------------------------------------------------------------------------

typedef struct Action Action_t;

typedef signal_t action_arg_t;

/**
 * Generic action handler interface
 *  - please note, that handler can be executed from context of interrupt service or from different process
 *  - allocation of resources when resource management is enabled might result in resource being mapped to resource list
 *  of process other than owner of actual action
 *  - in principle any calls that require running_process to match action owner should be avoided (all possibly blocking calls)
 *  - by default return true to remove action from list it is linked to (depends on particular action executor)
 */
typedef bool (*action_handler_t)(action_arg_t, action_arg_t);

/**
 * Generic action executor interface
 */
typedef void (*action_executor_t)(Action_t *_this, signal_t signal);

/**
 * Chainable action descriptor
 */
struct Action {
    // resource, sorted round-robin queue
    Sorted_queue_item_t _sortable;
    // function to be called on dispose
    dispose_function_t _dispose_hook;

    // -------- state --------
    // action handler and arguments
    action_handler_t handler;
    action_arg_t arg_1, arg_2;

    // -------- abstract public --------
    action_executor_t trigger;

};

void action_register(Action_t *action, dispose_function_t dispose_hook, action_executor_t trigger,
                        action_handler_t handler, action_arg_t arg_1, action_arg_t arg_2);

// -------------------------------------------------------------------------------------

/**
 * Pass given arguments to handler
 *  - if handler returns true, action shall be removed from queue it is (possibly) linked to
 */
void action_default_executor(Action_t *_this, signal_t signal);


#endif /* _SYS_ACTION_H_ */
