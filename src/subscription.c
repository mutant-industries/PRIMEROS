// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <subscription.h>
#include <stddef.h>
#include <process.h>


// Subscription_t destructor
static dispose_function_t _subscription_dispose(Subscription_t *_this) {
    // cleanup on_publish action (in case resource management is disabled)
    dispose(&_this->_on_publish);

    return NULL;
}

// Subscription_t constructor
void subscription_register(Subscription_t *subscription, signal_handler_t on_publish, void *owner, bool persistent,
        signal_interceptor_t interceptor, Schedule_config_t *with_config) {

    // register on_publish action first so that subscription is disposed before on_publish action (with auto-dispose)
    action_signal_create(&subscription->_on_publish, NULL, on_publish, with_config, running_process);
    action_owner(&subscription->_on_publish) = owner;

    // parent constructor
    action_proxy_create(subscription, (dispose_function_t) _subscription_dispose, &subscription->_on_publish, persistent, interceptor);
    action_owner(subscription) = owner;

    // both actions have the same priority by default
    sorted_set_item_priority(subscription) = sorted_set_item_priority(&subscription->_on_publish);
}
