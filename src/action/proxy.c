// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action/proxy.h>
#include <stddef.h>


#define _intercept(_proxy, _signal_ptr) action_proxy_signal_interceptor(_proxy)(action_owner(_proxy), _signal_ptr)

// -------------------------------------------------------------------------------------

// Action_proxy_t constructor
void action_proxy_register(Action_proxy_t *proxy, dispose_function_t dispose_hook,
        Action_t *target, bool persistent, signal_interceptor_t interceptor) {

    action_create(proxy, dispose_hook, action_proxy_trigger, interceptor);

    proxy->_target = target;
    proxy->_persistent = persistent;
}

// -------------------------------------------------------------------------------------

void action_proxy_trigger(Action_proxy_t *_this, signal_t signal) {
    Action_t *target;

    // execute signal interceptor if set
    if (action_proxy_signal_interceptor(_this) && ! _intercept(_this, &signal)) {
        return;
    }

    // just proxy trigger to target action
    if (target = _this->_target) {
        action_trigger(target, signal);
    }

    // if not persistent, proxy shall be triggered just once
    if ( ! _this->_persistent) {
        action_release(_this);
    }
}
