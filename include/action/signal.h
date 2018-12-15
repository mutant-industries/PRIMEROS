/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Inter-process signal pass on trigger
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_ACTION_SIGNAL_H_
#define _SYS_ACTION_SIGNAL_H_

#include <stddef.h>
#include <action.h>
#include <defs.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define action_signal(_signal) ((Action_signal_t *) (_signal))

/**
 * Action signal public API access
 */
#define action_signal_create(...) _ACTION_SIGNAL_CREATE_GET_MACRO(__VA_ARGS__, _action_signal_create_5, _action_signal_create_4)(__VA_ARGS__)

//<editor-fold desc="variable-args - action_signal_create()">
#define _ACTION_SIGNAL_CREATE_GET_MACRO(_1,_2,_3,_4,_5,NAME,...) NAME
#define _action_signal_create_4(_signal, _dispose_hook, _context, _handler) \
    action_signal_register(action_signal(_signal), _dispose_hook, _context, ((action_handler_t) (_handler)), NULL)
#define _action_signal_create_5(_signal, _dispose_hook, _context, _handler, _with_config) \
    action_signal_register(action_signal(_signal), _dispose_hook, _context, ((action_handler_t) (_handler)), _with_config)
//</editor-fold>

// getter, setter
#define action_signal_input_attr arg_2
#define action_signal_input(_action) action_attr(_action, action_signal_input_attr)

// -------------------------------------------------------------------------------------

typedef bool (*signal_handler_t)(void *owner, signal_t signal);

/**
 * Action executed within context of linked process
 */
typedef struct Action_signal {
    // resource, insert itself to pending_signals of linked process on trigger
    Action_t _triggerable;
    // linked execution context
    Process_control_block_t *_execution_context;
    // config for wakeup from blocking states
    Schedule_config_t _schedule_config;

} Action_signal_t;

/**
 * Initialize signal action
 * - on trigger insert itself to pending_signals of 'context' and schedule it if it is in waiting state
 * - 'handler' is executed within 'context' with two arguments - action owner and signal this action was triggered with
 * - action owner can be set via action_owner('signal') macro
 */
void action_signal_register(Action_signal_t *signal, dispose_function_t dispose_hook, Process_control_block_t *context,
        action_handler_t handler, Schedule_config_t *with_config);

// -------------------------------------------------------------------------------------

/**
 * Enqueue itself to pending_signals queue on linked process and schedule it if it is in waiting state
 */
void signal_trigger(Action_signal_t *_this, signal_t signal);


#endif /* _SYS_ACTION_SIGNAL_H_ */
