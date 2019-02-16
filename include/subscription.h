/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Subscription - handling of signals that originate in independent context within context of owner process
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_SUBSCRIPTION_H_
#define _SYS_SUBSCRIPTION_H_

#include <stdbool.h>
#include <stddef.h>
#include <action/signal.h>
#include <action/proxy.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define subscription(_subscription) ((Subscription_t *) (_subscription))

/**
 * Subscription public API access
 */
#define subscription_create(...) _SUBSCRIPTION_CREATE_GET_MACRO(__VA_ARGS__, __sc_6, __sc_5, __sc_4, __sc_3)(__VA_ARGS__)

//<editor-fold desc="variable-args - subscription_create()">
#define _SUBSCRIPTION_CREATE_GET_MACRO(_1,_2,_3,_4,_5,_6,NAME,...) NAME
#define __sc_3(_subscription, _on_publish, _owner) \
    subscription_register(subscription(_subscription), ((signal_handler_t) (_on_publish)), _owner, true, NULL, NULL)
#define __sc_4(_subscription, _on_publish, _owner, _persistent) \
    subscription_register(subscription(_subscription), ((signal_handler_t) (_on_publish)), _owner, _persistent, NULL, NULL)
#define __sc_5(_subscription, _on_publish, _owner, _persistent, _interceptor) \
    subscription_register(subscription(_subscription), ((signal_handler_t) (_on_publish)), _owner, _persistent, ((signal_interceptor_t) (_interceptor)), NULL)
#define __sc_6(_subscription, _on_publish, _owner, _persistent, _interceptor, _with_config) \
    subscription_register(subscription(_subscription), ((signal_handler_t) (_on_publish)), _owner, _persistent, ((signal_interceptor_t) (_interceptor)), _with_config)
//</editor-fold>

// getter, setter
#define subscription_on_publish_signal(_subscription) (&(subscription(_subscription)->_on_publish))

// -------------------------------------------------------------------------------------

/**
 * Subscription - proxy to signal handled within context of owner process
 */
typedef struct Subscription {
    // resource, intercept signal and trigger on_publish on trigger()
    Action_proxy_t _proxy;
    // signal handled within context of owner process
    Action_signal_t _on_publish;

} Subscription_t;

/**
 * Initialize subscription
 *  - subscription is custom handler ('on_publish') executed when triggered within context of process that created it
 *  - two arguments are passed to handler: 'owner' and signal the subscription was triggered with
 *  - subscription is nothing but proxy to signal action therefore process must be in waiting state to execute subscription handler
 *  - subscription has optional signal interceptor, that can be used to filter or aggregate signals, typical usage:
 *    - subscription is triggered by multiple sources where each source is identified by specific signal and process
 * wants to execute handler when all sources are triggered (waiting for multiple events)
 *    - subscription is triggered by multiple sources identified by specific signal, process wants to execute handler
 * when specific signal comes and wants to filter out others
 *    - process wants to keep track of how many times was specific subscription triggered - subscription itself
 * has no signal buffer and can be triggered faster that process can handle it
 *    -> interceptor is executed within context where subscription is triggered, not within owner process
 *  - subscription cannot be at the same time subscribed to two or more events, however two subscriptions can have
 * common signal interceptor
 *  - subscription is persistent by default, which means that when it is triggered, it is not removed from queue
 * it is (possibly) linked to - to subscribe to event and process just one signal set persistent to false
 *  - subscription inherits original priority of process that created it or priority set in 'with_config' if higher
 *    -> subscription handler is always processed at least with this priority
 */
void subscription_register(Subscription_t *subscription, signal_handler_t on_publish, void *owner, bool persistent,
        signal_interceptor_t interceptor, Schedule_config_t *with_config);


#endif /* _SYS_SUBSCRIPTION_H_ */
