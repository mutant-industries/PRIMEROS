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
#include <defs.h>
#include <action/queue.h>
#include <action/signal.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define semaphore(_semaphore) ((Semaphore_t *) (_semaphore))

/**
 * Semaphore public API access
 */
#define semaphore_create(...) _SEMAPHORE_CREATE_GET_MACRO(__VA_ARGS__, _semaphore_create_3, _semaphore_create_2, _semaphore_create_1)(__VA_ARGS__)
#define semaphore_try_acquire(_semaphore) semaphore(_semaphore)->try_acquire(semaphore(_semaphore))
#define semaphore_acquire(...) _SEMAPHORE_ACQUIRE_GET_MACRO(__VA_ARGS__, _semaphore_acquire_3, _semaphore_acquire_2, _semaphore_acquire_1)(__VA_ARGS__)
#define semaphore_acquire_async(_semaphore, _action) semaphore(_semaphore)->acquire_async(semaphore(_semaphore), action(_action))
#define semaphore_signal(_semaphore, _signal) action_trigger(action(_semaphore), _signal)

//<editor-fold desc="variable-args - semaphore_create()">
#define _SEMAPHORE_CREATE_GET_MACRO(_1,_2,_3,NAME,...) NAME
#ifndef __SIGNAL_PROCESSOR_DISABLE__
#define _semaphore_create_1(_semaphore) semaphore_register(semaphore(_semaphore), 0, &signal_processor)
#define _semaphore_create_2(_semaphore, _initial_permits_cnt) semaphore_register(semaphore(_semaphore), _initial_permits_cnt, &signal_processor)
#endif
#define _semaphore_create_3(_semaphore, _initial_permits_cnt, _context) semaphore_register(semaphore(_semaphore), _initial_permits_cnt, _context)
//</editor-fold>
//<editor-fold desc="variable-args - semaphore_acquire()">
#define _SEMAPHORE_ACQUIRE_GET_MACRO(_1,_2,_3,NAME,...) NAME
#define _semaphore_acquire_1(_semaphore) semaphore(_semaphore)->acquire(semaphore(_semaphore), NULL, NULL)
#define _semaphore_acquire_2(_semaphore, _timeout) semaphore(_semaphore)->acquire(semaphore(_semaphore), _timeout, NULL)
#define _semaphore_acquire_3(_semaphore, _timeout, _with_config) semaphore(_semaphore)->acquire(semaphore(_semaphore), _timeout, _with_config)
//</editor-fold>

// getter, setter
#define semaphore_permits_cnt(_semaphore) action_signal_unhandled_trigger_count(_semaphore)

/**
 * Semaphore public API return codes
 */
#define SEMAPHORE_SUCCESS           KERNEL_API_SUCCESS
#define SEMAPHORE_DISPOSED          KERNEL_DISPOSED_RESOURCE_ACCESS
#define SEMAPHORE_INVALID_ARGUMENT  KERNEL_API_INVALID_ARGUMENT
#define SEMAPHORE_NO_PERMITS        signal(1)
#define SEMAPHORE_ACQUIRE_TIMEOUT   KERNEL_API_TIMEOUT

// -------------------------------------------------------------------------------------

typedef struct Semaphore Semaphore_t;

/**
 * Counting semaphore
 */
struct Semaphore {
    // resource, semaphore_signal() on trigger
    Action_signal_t _signalable;

    // -------- state --------
    // queue of processes (actions) waiting on this semaphore
    Action_queue_t _queue;

    // -------- public --------
    // non-blocking acquire
    signal_t (*try_acquire)(Semaphore_t *_this);
    // acquire a permit or block until one is available, return passed signal if blocking
    //  - reset priority according to given config before inserting to semaphore queue
    signal_t (*acquire)(Semaphore_t *_this, Time_unit_t *timeout, Schedule_config_t *with_config);
    // non-blocking action enqueue - trigger if permit is available, trigger on signal otherwise
    signal_t (*acquire_async)(Semaphore_t *_this, Action_t *action);

};

// -------------------------------------------------------------------------------------

/**
 * Initialize semaphore with initial permits count
 *  - if blocking wait on semaphore with timeout is to be used, then 'context' should be default signal processor
 *  to avoid spurious wakeup
 */
void semaphore_register(Semaphore_t *semaphore, uint16_t initial_permits_cnt, Process_control_block_t *context);


#endif /* _SYS_SYNC_SEMAPHORE_H_ */
