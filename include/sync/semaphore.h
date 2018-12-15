/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Counting semaphore
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_SYNC_SEMAPHORE_H_
#define _SYS_SYNC_SEMAPHORE_H_

#include <stddef.h>
#include <stdint.h>
#include <action.h>
#include <action/queue.h>
#include <defs.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define semaphore(_semaphore) ((Semaphore_t *) (_semaphore))

/**
 * Semaphore public API access
 */
#define semaphore_try_acquire(_semaphore) semaphore(_semaphore)->try_acquire(semaphore(_semaphore))
#define semaphore_acquire(...) _SEMAPHORE_ACQUIRE_GET_MACRO(__VA_ARGS__, _semaphore_acquire_2, _semaphore_acquire_1)(__VA_ARGS__)
#define semaphore_acquire_async(_semaphore, _action) semaphore(_semaphore)->acquire_async(semaphore(_semaphore), action(_action))
#define semaphore_signal(_semaphore, _signal) action_trigger(action(_semaphore), _signal)

//<editor-fold desc="variable-args - semaphore_acquire()">
#define _SEMAPHORE_ACQUIRE_GET_MACRO(_1,_2,NAME,...) NAME
#define _semaphore_acquire_1(_semaphore) semaphore(_semaphore)->acquire(semaphore(_semaphore), NULL)
#define _semaphore_acquire_2(_semaphore, _with_config) semaphore(_semaphore)->acquire(semaphore(_semaphore), _with_config)
//</editor-fold>

// getter, setter
#define semaphore_permits_cnt(_semaphore) semaphore(_semaphore)->_permits_cnt

/**
 * Semaphore public API return codes
 */
#define SEMAPHORE_SUCCESS           KERNEL_API_SUCCESS
#define SEMAPHORE_DISPOSED          KERNEL_DISPOSED_RESOURCE_ACCESS
#define SEMAPHORE_INVALID_ARGUMENT  KERNEL_API_INVALID_ARGUMENT
#define SEMAPHORE_NO_PERMITS        signal(1)

// -------------------------------------------------------------------------------------

typedef struct Semaphore Semaphore_t;

/**
 * Counting semaphore
 */
struct Semaphore {
    // resource, semaphore_signal() on trigger
    Action_t _triggerable;

    // -------- state --------
    // queue of processes (actions) waiting on this semaphore
    Action_queue_t _queue;
    // current semaphore value
    uint16_t _permits_cnt;

    // -------- public --------
    // non-blocking acquire
    signal_t (*try_acquire)(Semaphore_t *_this);
    // acquire a permit or block until one is available, return passed signal if blocking
    //  - reset priority according to given config before inserting to semaphore queue
    signal_t (*acquire)(Semaphore_t *_this, Schedule_config_t *with_config);
    // non-blocking action enqueue - trigger if permit is available, trigger on signal otherwise
    signal_t (*acquire_async)(Semaphore_t *_this, Action_t *action);

};

// -------------------------------------------------------------------------------------

/**
 * Initialize semaphore with initial permits count
 *  - semaphore signal() always removes action from semaphore queue before action is triggered
 */
void semaphore_register(Semaphore_t *semaphore, uint16_t initial_permits_cnt);


#endif /* _SYS_SYNC_SEMAPHORE_H_ */
