// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action.h>
#include <driver/interrupt.h>
#include <action/queue.h>
#include <process.h>
#include <scheduler.h>


// Action_t destructor
static dispose_function_t _action_release(Action_t *_this) {

    // do nothing on trigger
    _this->trigger = (action_trigger_t) unsupported_after_disposed;

    // and remove itself from whatever queue it is (possibly) linked to
    action_release(_this);

    return _this->_dispose_hook;
}

// Action_t constructor
void action_register(Action_t *action, dispose_function_t dispose_hook, action_trigger_t trigger, action_handler_t handler) {

    // private
    zerofill(&action->_sortable);
    action->_dispose_hook = dispose_hook;

    // public
    action->trigger = trigger;
    action->handler = handler;

    __dispose_hook_register(action, _action_release);
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

bool action_default_set_priority(Action_t *action, priority_t priority) {

    typedef struct action_set_priority_request {
        Action_t *action;
        priority_t priority;

    } action_set_priority_request_t;

    volatile action_set_priority_request_t recursive_set_priority_request;
    bool highest_priority_placement = false;
    Action_queue_t *queue;

    interrupt_suspend();

    if ((queue = action_queue(deque_item_container(action)))) {

        if ( ! running_process->recursion_guard) {
            // this point reached just once in a single call
            running_process->recursion_guard = true;
            // store current process local storage
            void *process_local_bak = process_current_local();
            // prepare empty request
            recursive_set_priority_request.action = NULL;
            // and store it to local storage of current process
            process_current_local() = (void *) &recursive_set_priority_request;

            // execute priority change, which might initiate recursive call of this function
            highest_priority_placement = queue->_set_action_priority(action, priority, queue);

            while (recursive_set_priority_request.action) {
                // process (possible) recursive calls
                action = recursive_set_priority_request.action;
                priority = recursive_set_priority_request.priority;

                // break if no more recursive calls are made
                recursive_set_priority_request.action = NULL;

                // queue is always set if this point reached
                queue = action_queue(deque_item_container(action));

                // execute priority change, which might initiate another recursive call of this function
                queue->_set_action_priority(action, priority, queue);
            }

            // in case priority of some process was changed inside recursive call
            context_switch_trigger();

            // restore current process local storage
            process_current_local() = process_local_bak;
            // recursive call end
            running_process->recursion_guard = false;
        }
        else {
            // recursive call - just store request and return
            ((action_set_priority_request_t *) process_current_local())->action = action;
            ((action_set_priority_request_t *) process_current_local())->priority = priority;
        }
    }
    else {
        sorted_set_item_priority(action) = priority;
    }

    interrupt_restore();

    return highest_priority_placement;
}
