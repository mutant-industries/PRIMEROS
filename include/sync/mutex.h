/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Reentrant mutex with priority inheritance
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_SYNC_MUTEX_H_
#define _SYS_SYNC_MUTEX_H_

#include <stdint.h>
#include <action.h>
#include <action/queue.h>
#include <defs.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define mutex(_mutex) ((Mutex_t *) (_mutex))

/**
 * Mutex public API access
 */
#define mutex_try_lock(_mutex) mutex(_mutex)->try_lock(mutex(_mutex))
#define mutex_lock(...) _MUTEX_LOCK_GET_MACRO(__VA_ARGS__, _mutex_lock_2, _mutex_lock_1)(__VA_ARGS__)
#define mutex_unlock(_mutex) mutex(_mutex)->unlock(mutex(_mutex))

//<editor-fold desc="variable-args - mutex_lock()" >
#define _MUTEX_LOCK_GET_MACRO(_1,_2,NAME,...) NAME
#define _mutex_lock_1(_mutex) mutex(_mutex)->lock(mutex(_mutex), NULL)
#define _mutex_lock_2(_mutex, __with_config) mutex(_mutex)->lock(mutex(_mutex), __with_config)
//</editor-fold>

/**
 * Mutex public API return codes
 */
#define MUTEX_SUCCESS           KERNEL_API_SUCCESS
#define MUTEX_DISPOSED          KERNEL_DISPOSED_RESOURCE_ACCESS
#define MUTEX_LOCKED            signal(1)
#define MUTEX_INVALID_OWNER     signal(2)

// -------------------------------------------------------------------------------------

typedef struct Mutex Mutex_t;

/**
 * Recursive mutex
 */
struct Mutex {
    // resource, release() on trigger
    Action_t _releasable;

    // -------- state --------
    // queue of processes blocked on this mutex
    Action_queue_t _queue;
    // count how many times has owner acquired current mutex
    uint16_t _nesting_cnt;

    // -------- protected --------
    void (*_lock_timeout)(Mutex_t *_this, Process_control_block_t *process);

    // -------- public --------
    // non-blocking lock
    signal_t (*try_lock)(Mutex_t *_this);
    // acquire lock or block until lock available
    // - when lock is acquired, with_config shall be used to schedule process
    // - if mutex is locked, reset priority according to given config before inserting to mutex queue
    //   - if this process has highest priority in mutex queue, priority of mutex is set to that priority
    //   - mutex owner priority is always set at least to the priority of mutex itself
    signal_t (*lock)(Mutex_t *_this, Schedule_config_t *with_config);
    // release lock, reset priority and wakeup next waiting process
    // - also change priority of mutex to priority of first process in mutex queue or to 0 if mutex queue is empty
    signal_t (*unlock)(Mutex_t *_this);

};

// -------------------------------------------------------------------------------------

void mutex_register(Mutex_t *mutex);


#endif /* _SYS_SYNC_MUTEX_H_ */
