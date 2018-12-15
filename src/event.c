// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <event.h>
#include <stdbool.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <process.h>

// -------------------------------------------------------------------------------------

#ifndef __EVENT_PROCESSOR_DISABLE__

#ifndef __EVENT_PROCESSOR_STACK_SIZE__
#define __EVENT_PROCESSOR_STACK_SIZE__        0xFF
#endif

static Process_control_block_t _event_processor;
static uint8_t _event_processor_stack[__EVENT_PROCESSOR_STACK_SIZE__];

#endif /* __EVENT_PROCESSOR_DISABLE__ */

// -------------------------------------------------------------------------------------

static bool _event_dispatch(Event_t *_this, signal_t signal) {
    // trigger all event subscriptions
    action_queue_trigger_all(&_this->_subscription_list, signal);
    // always return true - do not break event processor loop
    return true;
}

static void _trigger_dummy() {
    // no action when subscription list is empty
}

// -------------------------------------------------------------------------------------

static signal_t _subscribe(Event_t *_this, Action_t *action) {

    interrupt_suspend();

    // insert action to subscription list
    action_queue_insert(&_this->_subscription_list, action);
    // subscription list no longer empty - set trigger
    action(_this)->trigger = (action_trigger_t) signal_trigger;

    interrupt_restore();

    return EVENT_SUCCESS;
}

void _subscription_released_hook(Event_t *owner, Action_t *_, Action_queue_t *origin) {
    // interrupts are disabled already

    if (action_queue_is_empty(origin)) {
        action(owner)->trigger = (action_trigger_t) _trigger_dummy;

        // last subscription released itself during event handler - avoid event priority drop to 0
        if (deque_item_container(owner)) {
            action_queue_on_head_priority_changed(&owner->_subscription_list) = NULL;
        }
    }
}

void _event_released_hook(void *owner, Event_t *event, Action_queue_t *origin) {
    // interrupts are disabled already

    // restore default event priority inheritance when event handler finished
    action_queue_on_head_priority_changed(&event->_subscription_list) = (head_priority_changed_hook_t) action_default_set_priority;
    // make sure priority state is always consistent
    sorted_set_item_priority(event) = action_queue_get_head_priority(&event->_subscription_list);
}

static signal_t _wait(Event_t *_this, Schedule_config_t *with_config) {

    interrupt_suspend();

    // initiate blocking wait on event, apply given config
    suspend(true, with_config, NULL, &_this->_subscription_list);
    // subscription list no longer empty - set trigger
    action(_this)->trigger = (action_trigger_t) signal_trigger;

    interrupt_restore();

    return running_process->blocked_state_signal;
}

// -------------------------------------------------------------------------------------

// Event_t destructor
static dispose_function_t _event_release(Event_t *_this) {

    // do nothing on subscribe and disable blocking wait
    _this->wait = (signal_t (*)(Event_t *, Schedule_config_t *)) unsupported_after_disposed;
    _this->subscribe = (signal_t (*)(Event_t *, Action_t *)) unsupported_after_disposed;

    // disable event inheriting subscription list priority
    action_queue_on_head_priority_changed(&_this->_subscription_list) = NULL;
    // disable dummy trigger setter
    action_queue_on_action_released(&_this->_subscription_list) = NULL;

    // close subscription list with disposed signal
    action_queue_close(&_this->_subscription_list, EVENT_DISPOSED);

    return NULL;
}

// Event_t constructor
void event_register(Event_t *event, Process_control_block_t *context) {
    Process_control_block_t *execution_context;

#ifndef __EVENT_PROCESSOR_DISABLE__
    execution_context = context ?: &_event_processor;
#else
    // no context given, no default context exists
    if ( ! (execution_context = context)) {
        return;
    }
#endif

    action_signal_create(event, (dispose_function_t) _event_release, execution_context, _event_dispatch);

    // no subscriptions yet therefore nothing to trigger
    action(event)->trigger = (action_trigger_t) _trigger_dummy;

    // inherit priority of subscription list, init dummy trigger setter
    action_queue_create(&event->_subscription_list, true, false, event,
            action_default_set_priority, _subscription_released_hook);

    // public
    event->subscribe = _subscribe;
    event->wait = _wait;
}

// -------------------------------------------------------------------------------------

#ifndef __EVENT_PROCESSOR_DISABLE__

void event_processor_init() {

    // event processor inherits priority of pending events
    _event_processor.create_config.priority = 0;
    _event_processor.create_config.stack_addr_low = (data_pointer_register_t) _event_processor_stack;
    _event_processor.create_config.stack_size = __EVENT_PROCESSOR_STACK_SIZE__;
    _event_processor.create_config.entry_point = (process_entry_point_t) wait;

    // create event processor
    process_create(&_event_processor);
    // and just set it to 'waiting for event' state, no need to schedule it
    process_waiting(&_event_processor) = true;

    // set hook on event handler finished event
    action_queue_on_action_released(&_event_processor.pending_signal_queue) = (action_released_hook_t) _event_released_hook;
}

#endif /* __EVENT_PROCESSOR_DISABLE__ */
