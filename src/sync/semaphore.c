// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <sync/semaphore.h>
#include <stddef.h>
#include <process.h>
#include <driver/interrupt.h>


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

static int16_t _acquire(Semaphore_t *_this, Process_schedule_config_t *config) {

    // possible priority shift for case when permits are available
    process_schedule(running_process, config);

    interrupt_suspend();

    running_process->blocked_state_return_value = SEMAPHORE_SUCCESS;

    if (_try_acquire(_this)) {
        // store / reset schedule config
        bytecopy(config, &running_process->schedule_config);
        // remove process from runnable queue and initiate context switch
        process_current_suspend();
        // standard enqueue to semaphore queue
        action_enqueue((Action_t *) running_process, &_this->_queue);
    }

    interrupt_restore();

    return running_process->blocked_state_return_value;
}

#ifndef __ASYNC_API_DISABLE__

static int16_t _acquire_async(Semaphore_t *_this, Action_t *action) {

    interrupt_suspend();

    // standard enqueue to semaphore queue
    action_enqueue(action, &_this->_queue);

    if (_try_acquire(_this)) {
        // execute action if
        _this->_queue->execute(_this->_queue, SEMAPHORE_SUCCESS);
    }

    interrupt_restore();

    return SEMAPHORE_SUCCESS;
}

#endif

static int16_t _signal(Semaphore_t *_this, int16_t signal) {

    interrupt_suspend();

    if ( ! _this->_queue) {
        _this->permits_cnt++;
    }
    else {
        _this->_queue->execute(_this->_queue, signal);
    }

    interrupt_restore();

    return SEMAPHORE_SUCCESS;
}

static int16_t _signal_all(Semaphore_t *_this, int16_t signal) {

    interrupt_suspend();

    action_execute_all(_this->_queue, signal);

    interrupt_restore();

    return SEMAPHORE_SUCCESS;
}

// -------------------------------------------------------------------------------------

// Semaphore_t destructor
static dispose_function_t _semaphore_release(Semaphore_t *_this) {

    interrupt_suspend();

    action_execute_all(_this->_queue, SEMAPHORE_DISPOSED);

    _this->try_acquire = (int16_t (*)(Semaphore_t *)) unsupported_after_disposed;
    _this->acquire = (int16_t (*)(Semaphore_t *, Process_schedule_config_t *)) unsupported_after_disposed;
#ifndef __ASYNC_API_DISABLE__
    _this->acquire_async = (int16_t (*)(Semaphore_t *, Action_t *)) unsupported_after_disposed;
#endif
    _this->signal = (int16_t (*)(Semaphore_t *, int16_t)) unsupported_after_disposed;
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
    semaphore->try_acquire = _try_acquire;
    semaphore->acquire = _acquire;
#ifndef __ASYNC_API_DISABLE__
    semaphore->acquire_async = _acquire_async;
#endif
    semaphore->signal = _signal;
    semaphore->signal_all = _signal_all;

    __dispose_hook_register(semaphore, _semaphore_release);
}
