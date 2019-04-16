// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action.h>
#include <driver/interrupt.h>
#include <action/queue.h>
#include <scheduler.h>


// Action_t destructor
static dispose_function_t _action_dispose(Action_t *_this) {

    // do nothing on trigger
    _this->trigger = (action_trigger_t) unsupported_after_disposed;

    // and remove itself from whatever queue it is (possibly) linked to
    action_release(_this);

    // disable on_released hook after action is released
    _this->on_released = NULL;

    return _this->_dispose_hook;
}

// Action_t constructor
void action_register(Action_t *action, dispose_function_t dispose_hook, action_trigger_t trigger, action_handler_t handler) {

    zerofill(action);

    // private
    action->_dispose_hook = dispose_hook;

    // public
    action->trigger = trigger;
    action->handler = handler;

    __dispose_hook_register(action, _action_dispose);
}

// -------------------------------------------------------------------------------------

void action_default_trigger(Action_t *_this, signal_t signal) {

    if ( ! _this->handler(action_owner(_this), signal)) {
        action_release(_this);
    }
}

void action_default_release(Action_t *action) {
    Action_queue_t *queue;

    interrupt_suspend();

    if ((queue = action_queue(deque_item_container(action)))) {
        queue->_release(action);
    }

    interrupt_restore();
}

// -------------------------------------------------------------------------------------

typedef struct action_set_priority_request {
    Action_t *action;
    priority_t priority;

} action_set_priority_request_t;

static volatile action_set_priority_request_t _recursive_set_priority_request;
static bool _recursion_guard;

bool action_default_set_priority(Action_t *action, priority_t priority) {

    bool highest_priority_placement = false;
    Action_queue_t *queue;

    interrupt_suspend();

    if ((queue = action_queue(deque_item_container(action)))) {

        if ( ! _recursion_guard) {
            // this point reached just once in a single call
            _recursion_guard = true;
            // prepare empty request
            _recursive_set_priority_request.action = NULL;

            // execute priority change, which might initiate recursive call of this function
            highest_priority_placement = queue->_set_action_priority(action, priority, queue);

            while (_recursive_set_priority_request.action) {
                // process (possible) recursive calls
                action = _recursive_set_priority_request.action;
                priority = _recursive_set_priority_request.priority;

                // break if no more recursive calls are made
                _recursive_set_priority_request.action = NULL;

                // queue is always set if this point reached
                queue = action_queue(deque_item_container(action));

                // execute priority change, which might initiate another recursive call of this function
                queue->_set_action_priority(action, priority, queue);
            }

            // recursive call end
            _recursion_guard = false;

            // in case priority of some process was changed inside recursive call
            context_switch_trigger();
        }
        else {
            // recursive call - just store request and return (stack usage optimization)
            _recursive_set_priority_request.action = action;
            _recursive_set_priority_request.priority = priority;
        }
    }
    else {
        sorted_set_item_priority(action) = priority;
    }

    interrupt_restore();

    return highest_priority_placement;
}
