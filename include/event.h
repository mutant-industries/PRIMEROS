/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Eventing - infrastructure for signal passing between independent contexts
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_EVENT_H_
#define _SYS_EVENT_H_

#include <stddef.h>
#include <action/signal.h>
#include <action/queue.h>
#include <defs.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define event(_event) ((Event_t *) (_event))

/**
 * Event public API access
 */
#define event_create(...) _EVENT_CREATE_GET_MACRO(__VA_ARGS__, _event_create_2, _event_create_1)(__VA_ARGS__)
#define event_subscribe(_event, _action) event(_event)->subscribe(event(_event), action(_action))
#define event_wait(...) _EVENT_WAIT_GET_MACRO(__VA_ARGS__, _event_wait_2, _event_wait_1)(__VA_ARGS__)
#define event_trigger(_event, _signal) action_trigger(_event, _signal)

//<editor-fold desc="variable-args - event_create()">
#define _EVENT_CREATE_GET_MACRO(_1,_2,NAME,...) NAME
#ifndef __EVENT_PROCESSOR_DISABLE__
#define _event_create_1(_event) event_register(event(_event), NULL)
#endif
#define _event_create_2(_event, _context) event_register(event(_event), _context)
//</editor-fold>
//<editor-fold desc="variable-args - event_wait()">
#define _EVENT_WAIT_GET_MACRO(_1,_2,NAME,...) NAME
#define _event_wait_1(_event) event(_event)->wait(event(_event), NULL)
#define _event_wait_2(_event, _with_config) event(_event)->wait(event(_event), _with_config)
//</editor-fold>

/**
 * Event public API return codes
 */
#define EVENT_SUCCESS           KERNEL_API_SUCCESS
#define EVENT_DISPOSED          KERNEL_DISPOSED_RESOURCE_ACCESS

// -------------------------------------------------------------------------------------

typedef struct Event Event_t;

/**
 * Event - action list executed within context of linked process after triggered
 */
struct Event {
    // resource, event_dispatch() on trigger
    Action_signal_t _signalable;
    // list of actions subscribed to this event
    Action_queue_t _subscription_list;

    // -------- public --------
    // add action to subscription list
    signal_t (*subscribe)(Event_t *_this, Action_t *);
    // blocking wait for event
    signal_t (*wait)(Event_t *_this, Schedule_config_t *with_config);

};

/**
 * Initialize event
 *  - event is simple signal action with custom handler, therefore process within which shall the handler
 * be processed ('context' or default event processor) inherits priority of event and only handles events in waiting state
 *  - event itself inherits priority of it's subscription list
 *  - handler of first event is always finished before starting handler of another event (within single execution context)
 *    -> even if priority of first event becomes lower than priority of another event during execution of it's handler
 *  - handlers of two events with the same priority are always handled in order they were triggered
 *  - if event has no subscriptions then event_trigger() has no effect (no context switch is initiated)
 */
void event_register(Event_t *event, Process_control_block_t *context);

// -------------------------------------------------------------------------------------

/**
 * Start process the context of which shall be used as default context to trigger actions subscribed to events
 */
void event_processor_init(void);


#endif /* _SYS_EVENT_H_ */
