/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Signal proxy & signal interceptor
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_ACTION_PROXY_H_
#define _SYS_ACTION_PROXY_H_

#include <stdbool.h>
#include <stddef.h>
#include <action.h>

// -------------------------------------------------------------------------------------

#define action_proxy(_action_proxy) ((Action_proxy_t *) (_action_proxy))

/**
 * Action proxy public API access
 */
#define action_proxy_create(...) _ACTION_PROXY_CREATE_GET_MACRO(__VA_ARGS__, _action_proxy_create_5, _action_proxy_create_4, _action_proxy_create_3)(__VA_ARGS__)

//<editor-fold desc="variable-args - action_proxy_create()">
#define _ACTION_PROXY_CREATE_GET_MACRO(_1,_2,_3,_4,_5,NAME,...) NAME
#define _action_proxy_create_3(_proxy, _dispose_hook, _target) \
    action_proxy_register(action_proxy(_proxy), _dispose_hook, action(_target), true, NULL)
#define _action_proxy_create_4(_proxy, _dispose_hook, _target, _persistent) \
    action_proxy_register(action_proxy(_proxy), _dispose_hook, action(_target), _persistent, NULL)
#define _action_proxy_create_5(_proxy, _dispose_hook, _target, _persistent, _interceptor) \
    action_proxy_register(action_proxy(_proxy), _dispose_hook, action(_target), _persistent, ((signal_interceptor_t) (_interceptor)))
//</editor-fold>

// getter, setter
#define action_proxy_signal_interceptor(_proxy) action_handler(_proxy)

// -------------------------------------------------------------------------------------

/**
 * Generic signal interceptor interface (action handler interface subtype)
 *  - return false to ignore signal
 */
typedef bool (*signal_interceptor_t)(void *owner, signal_t *signal);

/**
 * Action proxy
 */
typedef struct Action_proxy {
    // resource, intercept and pass signal to target action on trigger
    Action_t _triggerable;
    // action that shall be triggered on trigger
    Action_t *_target;
    // if not set, proxy shall release itself from queue it is (possibly) linked to on trigger
    bool _persistent;

} Action_proxy_t;


void action_proxy_register(Action_proxy_t *proxy, dispose_function_t dispose_hook,
        Action_t *target, bool persistent, signal_interceptor_t interceptor);

// -------------------------------------------------------------------------------------

/**
 * Trigger action proxy
 *  - filter (intercept) input signal first if interceptor function set
 *  - trigger linked action
 *  - remove itself from queue it is (possibly) linked to if not persistent
 */
void proxy_trigger(Action_proxy_t *_this, signal_t signal);


#endif /* _SYS_ACTION_PROXY_H_ */
