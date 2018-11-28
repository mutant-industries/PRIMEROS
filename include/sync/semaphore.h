/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Counting semaphore
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_SYNC_SEMAPHORE_H_
#define _SYS_SYNC_SEMAPHORE_H_

#include <stdint.h>
#include <action.h>

// -------------------------------------------------------------------------------------

/**
 * Semaphore public api return codes
 */
#define SEMAPHORE_SUCCESS           KERNEL_API_SUCCESS
#define SEMAPHORE_DISPOSED          KERNEL_DISPOSED_RESOURCE_ACCESS
#define SEMAPHORE_NO_PERMITS        (1)

// -------------------------------------------------------------------------------------

/**
 * Counting semaphore
 */
typedef struct Semaphore {
    // enable dispose(Semaphore_t *)
    Disposable_t _disposable;

    // -------- state --------
    // queue of actions waiting on this semaphore
    Action_t *_queue;
    // semaphore state ~ permits count
    uint16_t permits_cnt;

    // -------- public api --------
    // non-blocking acquire
    int16_t (*try_acquire)(struct Semaphore *_this);
    // acquire a permit or block until one is available, return passed signal if blocking
    //  - reset priority before inserting to semaphore queue to original process priority
    //  - reschedule according to given config if set on return
    int16_t (*acquire)(struct Semaphore *_this, Process_schedule_config_t *config);
#ifndef __ASYNC_API_DISABLE__
    // non-blocking action enqueue - execute if permit is available, execute on signal otherwise
    int16_t (*acquire_async)(struct Semaphore *_this, Action_t *action);
#endif
    // schedule first process in queue and pass signal to it, or increment semaphore permits count if queue empty
    int16_t (*signal)(struct Semaphore *_this, int16_t signal);
    // atomically wakeup all processes blocked on semaphore
    int16_t (*signal_all)(struct Semaphore *_this, int16_t signal);

} Semaphore_t;

// -------------------------------------------------------------------------------------

void semaphore_register(Semaphore_t *semaphore, uint16_t initial_permits_cnt);


#endif /* _SYS_SYNC_SEMAPHORE_H_ */
