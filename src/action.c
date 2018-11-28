// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action.h>
#include <driver/interrupt.h>
#include <process.h>


// -------------------------------------------------------------------------------------

bool action_enqueue(Action_t *action, Action_t **queue) {
    List_item_t *current, *action_list_item;
    bool highest_priority_placement = false;

    interrupt_suspend();

    // action queued already
    if (action->queue == queue) {
        highest_priority_placement = *queue == action;
    }
    // queue empty or highest priority action
    else if ( ! *queue || (*queue)->priority < action->priority) {
        list_add_last((List_item_t *) action, (List_item_t **) queue);
        // new head of the queue
        *queue = action;
        // enqueued action has highest priority
        highest_priority_placement = true;
    }
    else {
        current = (List_item_t *) *queue;

        // place behind the first element with higher or equal priority or place to end of queue
        while (((Action_t *) current->_next)->priority >= action->priority && current->_next != (List_item_t *) *queue) {
            current = current->_next;
        }

        action_list_item = (List_item_t *) action;

        action_list_item->_next = current->_next;
        action_list_item->_prev = current;
        current->_next->_prev = action_list_item;
        current->_next = action_list_item;
    }

    // link given queue to action
    action->queue = queue;

    interrupt_restore();

    return highest_priority_placement;
}

void action_dequeue(Action_t *action) {

    // sanity check
    if ( ! action->queue) {
        return;
    }

    interrupt_suspend();

    // remove action from queue
    list_remove((List_item_t *) action, (List_item_t **) action->queue);
    // ... and discard link to it
    action->queue = NULL;

    interrupt_restore();
}

// -------------------------------------------------------------------------------------

void action_execute_all(Action_t *queue, int16_t signal) {

    // sanity check, queue empty
    if ( ! queue) {
        return;
    }

    // suppose queue has round-robin chaining
    Action_t *last = (Action_t *) ((List_item_t *) queue)->_prev;
    Action_t *current, *next = queue;

    // ... also support simple linked chaining
    while ((current = next)) {
        // next has to be stored before current is executed, because execution might remove current from queue
        next = (Action_t *) ((List_item_t *) current)->_next;
        // execute handler and pass given signal
        current->execute(current, signal);
        // stop if last action was executed
        if (current == last) {
            break;
        }
    }
}

// -------------------------------------------------------------------------------------

void action_default_executor(Action_t *_this, int16_t signal) {

    if ( ! _this->handler(_this->arg_1, _this->arg_2) && _this->queue) {
        action_dequeue(_this);
    }
}

// -------------------------------------------------------------------------------------

// Action_t destructor
static dispose_function_t _action_release(Action_t *_this) {

    // just remove itself from whatever queue it is (possibly) linked to
    action_dequeue(_this);

    return _this->_dispose_hook;
}

// Action_t constructor
void action_register(Action_t *action, dispose_function_t dispose_hook, action_executor execute,
                        action_handler handler, void *arg_1, void *arg_2) {

    // private
    action->handler = handler;
    action->arg_1 = arg_1;
    action->arg_2 = arg_2;
    action->_dispose_hook = dispose_hook;

    // public api
    action->execute = execute;

    __dispose_hook_register(action, _action_release);
}
