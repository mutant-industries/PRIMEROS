// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action.h>
#include <action/queue.h>


void action_default_executor(Action_t *_this, signal_t signal) {

    if (_this->handler(_this, signal)) {
        remove(_this);
    }
}

// -------------------------------------------------------------------------------------

// Action_t destructor
static dispose_function_t _action_release(Action_t *_this) {

    // just remove itself from whatever queue it is (possibly) linked to
    action_remove(_this);

    return _this->_dispose_hook;
}

// Action_t constructor
void action_register(Action_t *action, dispose_function_t dispose_hook, action_executor_t trigger,
                        action_handler_t handler, action_arg_t arg_1, action_arg_t arg_2) {

    // private
    action->handler = handler;
    action->arg_1 = arg_1;
    action->arg_2 = arg_2;
    action->_dispose_hook = dispose_hook;

    // public
    action->trigger = trigger;

    __dispose_hook_register(action, _action_release);
}
