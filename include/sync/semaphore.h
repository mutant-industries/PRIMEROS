/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Counting semaphore
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_SYNC_SEMAPHORE_H_
#define _SYS_SYNC_SEMAPHORE_H_

#include <stdbool.h>
#include <stdint.h>
#include <defs.h>
#include <resource.h>

// -------------------------------------------------------------------------------------

/**
 * Semaphore public api return codes
 */
#define SEMAPHORE_SUCCESS           KERNEL_API_SUCCESS
#define SEMAPHORE_DISPOSED          KERNEL_DISPOSED_RESOURCE_ACCESS
#define SEMAPHORE_NO_PERMITS        (1)

// -------------------------------------------------------------------------------------

/**
 * Counting semaphore, public api
 */
typedef struct Semaphore {
    // enable dispose(Semaphore_t *)
    Disposable_t _disposable;

    // -------- state --------
    // queue of processes blocked on this semaphore
    Process_control_block_t *_queue;
    // semaphore state ~ permits count
    uint16_t permits_cnt;

    // -------- public api --------
    // non-blocking acquire
    int16_t (*try_acquire)(struct Semaphore *_this);
    // acquire a permit from this semaphore, block until one is available, return passed signal if blocking
    int16_t (*acquire)(struct Semaphore *_this);
    // schedule first process in queue and pass signal to it, or increment semaphore permits count if queue empty
    int16_t (*signal)(struct Semaphore *_this, int16_t signal);
    // atomically wakeup all processes blocked on semaphore
    int16_t (*signal_all)(struct Semaphore *_this, int16_t signal);

} Semaphore_t;

// -------------------------------------------------------------------------------------

void semaphore_register(Semaphore_t *semaphore, uint16_t initial_permits_cnt);


#endif /* _SYS_SYNC_SEMAPHORE_H_ */
