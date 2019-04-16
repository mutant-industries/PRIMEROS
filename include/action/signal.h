/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Inter-process signal pass on trigger
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_ACTION_SIGNAL_H_
#define _SYS_ACTION_SIGNAL_H_

#include <stddef.h>
#include <defs.h>
#include <action.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define action_signal(_signal) ((Action_signal_t *) (_signal))

/**
 * Action signal public API access
 */
#define action_signal_create(...) _ACTION_SIGNAL_CREATE_GET_MACRO(__VA_ARGS__, _action_signal_create_5, _action_signal_create_4)(__VA_ARGS__)
#define action_signal_handled_callback(_signal) action_signal(_signal)->on_handled(action_signal(_signal))

//<editor-fold desc="variable-args - action_signal_create()">
#define _ACTION_SIGNAL_CREATE_GET_MACRO(_1,_2,_3,_4,_5,NAME,...) NAME
#ifndef __SIGNAL_PROCESSOR_DISABLE__
#define _action_signal_create_4(_signal, _dispose_hook, _handler, _with_config) \
    action_signal_register(action_signal(_signal), _dispose_hook, ((signal_handler_t) (_handler)), _with_config, &signal_processor)
#endif
#define _action_signal_create_5(_signal, _dispose_hook, _handler, _with_config, _context) \
    action_signal_register(action_signal(_signal), _dispose_hook, ((signal_handler_t) (_handler)), _with_config, _context)
//</editor-fold>

// getter, setter
#define action_signal_input_attr arg_2
#define action_signal_input(_signal) action_attr(_signal, action_signal_input_attr)
#define action_signal_unhandled_trigger_count(_signal) action_signal(_signal)->_unhandled_trigger_count
#define action_signal_execution_context(_signal) action_signal(_signal)->_execution_context
#define action_signal_keep_priority_while_handled(_signal) action_signal(_signal)->_keep_priority_while_handled
#define action_signal_schedule_config(_signal) (&(action_signal(_signal)->_schedule_config))
#define action_signal_on_handled(_signal) action_signal(_signal)->on_handled

// -------------------------------------------------------------------------------------

typedef struct Action_signal Action_signal_t;

typedef bool (*signal_handler_t)(void *owner, signal_t signal);

/**
 * Action executed within context of linked process
 */
struct Action_signal {
    // resource, insert itself to pending_signals of linked process on trigger
    Action_t _triggerable;
    // incremented when triggered, decremented within (default) on_handled callback
    uint16_t _unhandled_trigger_count;
    // linked execution context
    Process_control_block_t *_execution_context;
    // if priority of signal changes while handled or signal removes itself from pending queue while handled,
    // then priority of execution context shall not drop below priority of signal it had before handler was called
    bool _keep_priority_while_handled;
    // general-purpose schedule config
    Schedule_config_t _schedule_config;
    // triggered after action handler is executed (interrupts are disabled during execution)
    // - if set, return true to stay in pending_signals queue
    bool (*on_handled)(Action_signal_t *_this);

};

/**
 * Initialize signal action
 *  - on trigger insert itself to pending_signals of 'context' and schedule it if it is in waiting state
 *  - 'handler' is executed within 'context' with two arguments - action owner and signal this action was triggered with
 *  - if triggered faster than handled then 'signal' parameter passed to handler is the last signal this action was triggered with
 *  - signal by default inherits priority of running process or priority given by schedule config if higher
 *  - 'context' inherits priority of all pending (unhandled) signals
 */
void action_signal_register(Action_signal_t *signal, dispose_function_t dispose_hook, signal_handler_t handler,
        Schedule_config_t *with_config, Process_control_block_t *context);

// -------------------------------------------------------------------------------------

/**
 * Enqueue itself to pending_signals queue on linked process and schedule it if it is in waiting state
 */
void signal_trigger(Action_signal_t *_this, signal_t signal);

/**
 * Default priority setter for signals that make use of schedule config
 *  - priority is set to min('priority_lowest', schedule_config->priority)
 *  - function signature corresponds to action_set_priority() for action type signal
 */
bool signal_set_priority(Action_signal_t *signal, priority_t priority_lowest);

// -------------------------------------------------------------------------------------

#ifndef __SIGNAL_PROCESSOR_DISABLE__

// general-purpose signal processor
extern Process_control_block_t signal_processor;

/**
 * Start general-purpose process the context of which can be used as default signal processor for:
 *  - actions subscribed to events
 *  - timed signal queue handlers
 *  - signal on semaphores
 *  - any other (user-defined) signals
 */
void signal_processor_init(void);

#endif


#endif /* _SYS_ACTION_SIGNAL_H_ */
