// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <sync/semaphore.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <process.h>


static signal_t _try_acquire(Semaphore_t *_this) {
    signal_t result = SEMAPHORE_SUCCESS;

    interrupt_suspend();

    if(_this->_permits_cnt) {
        _this->_permits_cnt--;
    }
    else {
        result = SEMAPHORE_NO_PERMITS;
    }

    interrupt_restore();

    return result;
}

static signal_t _acquire(Semaphore_t *_this, Schedule_config_t *with_config) {

    interrupt_suspend();
    // suspend running process if no permits are available, apply given config
    suspend((bool) _try_acquire(_this), with_config, SEMAPHORE_SUCCESS, &_this->_queue);

    interrupt_restore();

    return running_process->blocked_state_signal;
}

static signal_t _acquire_async(Semaphore_t *_this, Action_t *action) {
    bool permit_available = false;

    // sanity check
    if (action == action(running_process)) {
        return SEMAPHORE_INVALID_ARGUMENT;
    }

    interrupt_suspend();

    if (_try_acquire(_this)) {
        action_queue_insert(&_this->_queue, action);
    }
    else {
        // semaphore queue empty, no need to enqueue the action - it can be triggered in current context
        permit_available = true;
    }

    interrupt_restore();

    if (permit_available) {
        action_trigger(action, SEMAPHORE_SUCCESS);
    }

    return SEMAPHORE_SUCCESS;
}

static signal_t _signal(Semaphore_t *_this, signal_t signal) {
    Action_t *action_to_signal = NULL;

    interrupt_suspend();

    if ( ! (action_to_signal = action_queue_pop(&_this->_queue))) {
        _this->_permits_cnt++;
    }

    interrupt_restore();

    if (action_to_signal) {
        action_trigger(action_to_signal, signal);
    }

    return SEMAPHORE_SUCCESS;
}

// -------------------------------------------------------------------------------------

// Semaphore_t destructor
static dispose_function_t _semaphore_release(Semaphore_t *_this) {

    _this->acquire = (signal_t (*)(Semaphore_t *, Schedule_config_t *)) unsupported_after_disposed;
    _this->try_acquire = (signal_t (*)(Semaphore_t *)) unsupported_after_disposed;
    _this->acquire_async = (signal_t (*)(Semaphore_t *, Action_t *)) unsupported_after_disposed;

    action_queue_close(&_this->_queue, SEMAPHORE_DISPOSED);

    return NULL;
}

// Semaphore_t constructor
void semaphore_register(Semaphore_t *semaphore, uint16_t initial_permits_cnt) {

    action_create(semaphore, (dispose_function_t) _semaphore_release, _signal);
    action_queue_create(&semaphore->_queue, true, true);

    // state
    semaphore->_permits_cnt = initial_permits_cnt;

    // public
    semaphore->try_acquire = _try_acquire;
    semaphore->acquire = _acquire;
    semaphore->acquire_async = _acquire_async;
}
