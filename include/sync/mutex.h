/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Reentrant mutex with priority inheritance
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_SYNC_MUTEX_H_
#define _SYS_SYNC_MUTEX_H_

#include <stddef.h>
#include <stdint.h>
#include <defs.h>
#include <action.h>
#include <action/queue.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define mutex(_mutex) ((Mutex_t *) (_mutex))

/**
 * Mutex public API access
 */
#define mutex_try_lock(_mutex) mutex(_mutex)->try_lock(mutex(_mutex))
#define mutex_lock(...) _MUTEX_LOCK_GET_MACRO(__VA_ARGS__, _mutex_lock_3, _mutex_lock_2, _mutex_lock_1)(__VA_ARGS__)
#define mutex_unlock(_mutex) mutex(_mutex)->unlock(mutex(_mutex))

//<editor-fold desc="variable-args - mutex_lock()">
#define _MUTEX_LOCK_GET_MACRO(_1,_2,_3,NAME,...) NAME
#define _mutex_lock_1(_mutex) mutex(_mutex)->lock(mutex(_mutex), NULL, NULL)
#define _mutex_lock_2(_mutex, _timeout) mutex(_mutex)->lock(mutex(_mutex), _timeout, NULL)
#define _mutex_lock_3(_mutex, _timeout, _with_config) mutex(_mutex)->lock(mutex(_mutex), _timeout, _with_config)
//</editor-fold>

/**
 * Mutex public API return codes
 */
#define MUTEX_SUCCESS           KERNEL_API_SUCCESS
#define MUTEX_DISPOSED          KERNEL_DISPOSED_RESOURCE_ACCESS
#define MUTEX_LOCKED            signal(1)
#define MUTEX_INVALID_OWNER     signal(2)
#define MUTEX_LOCK_TIMEOUT      KERNEL_API_TIMEOUT

// -------------------------------------------------------------------------------------

typedef struct Mutex Mutex_t;

/**
 * Recursive mutex
 */
struct Mutex {
    // resource, release() on trigger
    Action_t _triggerable;

    // -------- state --------
    // queue of processes blocked on this mutex
    Action_queue_t _queue;
    // count how many times has owner acquired current mutex
    uint16_t _nesting_cnt;

    // -------- public --------
    // non-blocking lock
    signal_t (*try_lock)(Mutex_t *_this);
    // acquire lock or block until lock available
    // - if mutex is locked, reset priority according to given config before inserting to mutex queue
    //   - if current process has highest priority in mutex queue, priority of mutex is inherited
    //   - mutex owner priority is always set at least to the priority of mutex itself
    signal_t (*lock)(Mutex_t *_this, Time_unit_t *timeout, Schedule_config_t *with_config);
    // release mutex, reset priority and wakeup next waiting process
    // - also change priority of mutex to priority of first process in mutex queue or to 0 if mutex queue is empty
    signal_t (*unlock)(Mutex_t *_this);

};

// -------------------------------------------------------------------------------------

/**
 * Initialize mutex
 *  - mutex inherits priority of its queue (mutex priority equals queue head priority or 0 if queue is empty)
 *  - process inherits priority of 'on_exit_action_queue' which is where mutex is inserted when acquired {@see schedulable_state_reset}
 *  - if process is killed while it holds mutex or terminates without releasing it, then it is released automatically
 */
void mutex_register(Mutex_t *mutex);


#endif /* _SYS_SYNC_MUTEX_H_ */
