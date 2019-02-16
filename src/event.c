// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <event.h>
#include <stdbool.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <process.h>


static void (*_subscription_queue_release_parent)(Action_t *);

// -------------------------------------------------------------------------------------

static bool _event_dispatch(Event_t *_this, signal_t signal) {
    // trigger all event subscriptions
    action_queue_trigger_all(&_this->_subscription_list, signal);
    // stay in waiting loop
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

static signal_t _wait(Event_t *_this, Time_unit_t *timeout, Schedule_config_t *with_config) {

    interrupt_suspend();

    // initiate blocking wait on event, apply given config
    suspend(EVENT_WAIT_TIMEOUT, &_this->_subscription_list, timeout, with_config);
    // subscription list no longer empty - set trigger
    action(_this)->trigger = (action_trigger_t) signal_trigger;

    interrupt_restore();

    return running_process->blocked_state_signal;
}

// -------------------------------------------------------------------------------------

// subscription list release override
static void _subscription_list_release(Action_t *action) {
    // interrupts are disabled already

    // assert not null when this point reached
    Action_queue_t *origin = action_queue(deque_item_container(action));
    // event the queue belongs to, assert not null
    Event_t *owner = action_queue_owner(origin);

    // execute parent release
    _subscription_queue_release_parent(action);

    // do nothing on trigger if subscription list is empty
    if (action_queue_is_empty(origin)) {
        action(owner)->trigger = (action_trigger_t) _trigger_dummy;
    }
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
    action(event)->trigger = (action_trigger_t) _trigger_dummy;

    // inherit priority of subscription list and schedule config, init dummy trigger setter
    action_queue_create(&event->_subscription_list, true, false, event, signal_set_priority);
    // store subscription queue release original function pointer...
    _subscription_queue_release_parent = event->_subscription_list._release;
    // ...and override it
    event->_subscription_list._release = _subscription_list_release;

    // public
    event->subscribe = _subscribe;
    event->wait = _wait;
}
