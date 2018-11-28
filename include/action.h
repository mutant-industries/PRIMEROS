/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Action descriptor, action execution
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_ACTION_H_
#define _SYS_ACTION_H_

#include <stdbool.h>
#include <stdint.h>
#include <collection/list.h>
#include <defs.h>

// -------------------------------------------------------------------------------------

typedef struct Action Action_t;

/**
 * Generic action handler interface
 *  - handler can be executed from context of interrupt service or from different process
 *  - allocation of resources when resource management is enabled might result in resource being mapped to resource list
 *  of process other than owner of actual action
 *  - in principle any calls that require running_process to match action owner should be avoided (all possibly blocking calls)
 *  - by default return false to remove action from list it is linked to (depends on particular action executor)
 */
typedef bool (*action_handler)(void *, void *);

/**
 * Generic action executor interface
 */
typedef void (*action_executor)(Action_t *, int16_t signal);

/**
 * Chainable action descriptor
 */
struct Action {
    // enable round-robin chaining and dispose(Action_t *)
    List_item_t _chainable;
    // function to be called on dispose
    dispose_function_t _dispose_hook;

    // -------- state --------
    // action handler and arguments
    action_handler handler;
    void *arg_1, *arg_2;
    // the queue the the action is linked to
    struct Action **queue;
    // priority by which actions are sorted in descending order by default, {@see action_enqueue()}
    uint16_t priority;

    // -------- abstract public api --------
    action_executor execute;

};

void action_register(Action_t *action, dispose_function_t dispose_hook, action_executor execute,
                        action_handler handler, void *arg_1, void *arg_2);

// -------------------------------------------------------------------------------------

/**
 * Place action to given queue, order by priority and return true if given action has highest priority on given list
 */
bool action_enqueue(Action_t *action, Action_t **queue);

/**
 * Remove action from whatever queue it is (possibly) linked to
 */
void action_dequeue(Action_t *action);

/**
 * Execute all actions on given list with given signal
 */
void action_execute_all(Action_t *queue, int16_t signal);

// -------------------------------------------------------------------------------------

/**
 * Execute handler and pass handler arguments
 *  - if handler returns false, action shall be removed from queue it is linked to (if any)
 *  - signal is ignored
 */
void action_default_executor(Action_t *, int16_t signal);


#endif /* _SYS_ACTION_H_ */
