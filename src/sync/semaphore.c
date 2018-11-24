// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <sync/semaphore.h>
#include <stddef.h>
#include <process.h>
#include <driver/interrupt.h>
#include <collection/list.h>


// -------------------------------------------------------------------------------------

static void _release_process(Semaphore_t *_this, Process_control_block_t *process, int16_t signal) {

    // sanity check
    if (&_this->_queue != process->queue) {
        return;
    }

    // remove process from semaphore queue
    list_remove((List_item_t **) &_this->_queue, (List_item_t *) process);
    // pass signal to the process
    process->blocked_state_return_value = signal;
    // and reschedule process
    process_schedule(process);
}

// -------------------------------------------------------------------------------------

static int16_t _try_acquire(Semaphore_t *_this) {
    int16_t result = SEMAPHORE_NO_PERMITS;

    interrupt_suspend();

    if(_this->permits_cnt) {
        _this->permits_cnt--;

        result = SEMAPHORE_SUCCESS;
    }

    interrupt_restore();

    return result;
}

static int16_t _acquire(Semaphore_t *_this) {

    interrupt_suspend();

    running_process->blocked_state_return_value = SEMAPHORE_SUCCESS;

    if (_try_acquire(_this)) {
        // remove from actual process queue
        list_remove((List_item_t **) running_process->queue, (List_item_t *) running_process);
        // standard enqueue to semaphore queue
        process_enqueue(&_this->_queue, running_process);
        // initiate context switch
        yield();
    }

    interrupt_restore();

    return running_process->blocked_state_return_value;
}

static int16_t _signal(Semaphore_t *_this, int16_t signal) {

    interrupt_suspend();

    if ( ! _this->_queue) {
        _this->permits_cnt++;
    }
    else {
        _release_process(_this, _this->_queue, signal);
    }

    interrupt_restore();

    return SEMAPHORE_SUCCESS;
}

static int16_t _signal_all(Semaphore_t *_this, int16_t signal) {

    interrupt_suspend();

    while (_this->_queue) {
        _release_process(_this, _this->_queue, signal);
    }

    interrupt_restore();

    return SEMAPHORE_SUCCESS;
}

// -------------------------------------------------------------------------------------

// Semaphore_t destructor
static dispose_function_t _semaphore_release(Semaphore_t *_this) {

    interrupt_suspend();

    _signal_all(_this, SEMAPHORE_DISPOSED);

    _this->acquire = (int16_t (*)(Semaphore_t *)) unsupported_after_disposed;
    _this->signal = (int16_t (*)(Semaphore_t *, int16_t)) unsupported_after_disposed;
    _this->try_acquire = (int16_t (*)(Semaphore_t *)) unsupported_after_disposed;
    _this->signal_all = (int16_t (*)(Semaphore_t *, int16_t)) unsupported_after_disposed;

    interrupt_restore();

    return NULL;
}

// Semaphore_t constructor
void semaphore_register(Semaphore_t *semaphore, uint16_t initial_permits_cnt) {

    // state
    semaphore->_queue = NULL;
    semaphore->permits_cnt = initial_permits_cnt;

    // public api
    semaphore->acquire = _acquire;
    semaphore->signal = _signal;
    semaphore->try_acquire = _try_acquire;
    semaphore->signal_all = _signal_all;

    __dispose_hook_register(semaphore, _semaphore_release);
}
