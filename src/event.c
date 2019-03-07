// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <event.h>
#include <stdbool.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <process.h>


static bool _event_dispatch(Event_t *_this, signal_t signal) {
    // trigger all event subscriptions
    action_queue_trigger_all(&_this->_subscription_list, signal);
    // stay in waiting loop
    return true;
}

// -------------------------------------------------------------------------------------

static signal_t _event_subscribe(Event_t *_this, Action_t *action) {

    // sanity check
    if (action == action(running_process)) {
        return EVENT_INVALID_ARGUMENT;
    }

    // just insert action to subscription list
    action_queue_insert(&_this->_subscription_list, action);

    return EVENT_SUCCESS;
}

static signal_t _event_wait(Event_t *_this, Time_unit_t *timeout, Schedule_config_t *with_config) {

    interrupt_suspend();

    // initiate blocking wait on event, apply given config
    suspend(EVENT_WAIT_TIMEOUT, &_this->_subscription_list, timeout, with_config);

    interrupt_restore();

    return running_process->blocked_state_signal;
}

static void _event_trigger(Event_t *_this, signal_t signal) {

    interrupt_suspend();

    // only trigger if subscription list is not empty
    if ( ! action_queue_is_empty(&_this->_subscription_list)) {
        signal_trigger(action_signal(_this), signal);
    }

    interrupt_restore();
}

// -------------------------------------------------------------------------------------

// Event_t destructor
static dispose_function_t _event_dispose(Event_t *_this) {

    // do nothing on subscribe and disable blocking wait
    _this->wait = (signal_t (*)(Event_t *, Time_unit_t *, Schedule_config_t *)) unsupported_after_disposed;
    _this->subscribe = (signal_t (*)(Event_t *, Action_t *)) unsupported_after_disposed;

    // disable event inheriting subscription list priority
    action_queue_on_head_priority_changed(&_this->_subscription_list) = NULL;

    // close subscription list with disposed signal
    action_queue_close(&_this->_subscription_list, EVENT_DISPOSED);

    return NULL;
}

// Event_t constructor
void event_register(Event_t *event, Schedule_config_t *with_config, Process_control_block_t *context) {

    action_signal_create(event, (dispose_function_t) _event_dispose, _event_dispatch, with_config, context);
    // event does not inherit owner process priority by default
    sorted_set_item_priority(event) = action_signal_schedule_config(event)->priority;
    // no subscriptions yet therefore nothing to trigger
    action(event)->trigger = (action_trigger_t) _event_trigger;

    // inherit priority of subscription list and schedule config, init dummy trigger setter
    action_queue_create(&event->_subscription_list, true, false, event, signal_set_priority);

    // public
    event->subscribe = _event_subscribe;
    event->wait = _event_wait;
}
