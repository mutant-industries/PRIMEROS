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

    // store / reset schedule config
    bytecopy(with_config, action_schedule_config(running_process));

    interrupt_suspend();

    running_process->blocked_state_signal = SEMAPHORE_SUCCESS;

    if (_try_acquire(_this)) {
        // remove running process from runnable queue
        remove(running_process);
        // initiate context switch, priority reset
        schedulable_state_reset(running_process, false);
        // standard enqueue to semaphore queue
        enqueue(running_process, &_this->_queue);
    }
    else {
        // possible priority shift for case when permits are available
        process_schedule(running_process, SEMAPHORE_SUCCESS);
    }

    interrupt_restore();

    return running_process->blocked_state_signal;
}

#ifndef __ASYNC_API_DISABLE__

static signal_t _acquire_async(Semaphore_t *_this, Action_t *action) {
    bool permit_available = false;

    // sanity check
    if (action == action(running_process)) {
        return SEMAPHORE_INVALID_ARGUMENT;
    }

    interrupt_suspend();

    if (_try_acquire(_this)) {
        // standard enqueue to action priority queue
        enqueue(action, &_this->_queue);
    }
    else {
        // semaphore queue empty, no need to enqueue the action since it can be triggered without delay
        permit_available = true;
    }

    interrupt_restore();

    if (permit_available) {
        action_trigger(action, SEMAPHORE_SUCCESS);
    }

    return SEMAPHORE_SUCCESS;
}

#endif

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
    Action_t *action_to_signal;

    _this->try_acquire = (signal_t (*)(Semaphore_t *)) unsupported_after_disposed;
    _this->acquire = (signal_t (*)(Semaphore_t *, Schedule_config_t *)) unsupported_after_disposed;
#ifndef __ASYNC_API_DISABLE__
    _this->acquire_async = (signal_t (*)(Semaphore_t *, Action_t *)) unsupported_after_disposed;
#endif
    action(_this)->trigger = (action_executor_t) unsupported_after_disposed;

    while (action_to_signal = action_queue_pop(&_this->_queue)) {
        action_trigger(action_to_signal, SEMAPHORE_DISPOSED);
    }

    return NULL;
}

// Semaphore_t constructor
void semaphore_register(Semaphore_t *semaphore, uint16_t initial_permits_cnt) {

    // state
    action_register(action(semaphore), (dispose_function_t) _semaphore_release, (action_executor_t) _signal, NULL, NULL, NULL);
    action_queue_init(&semaphore->_queue, true);
    semaphore->_permits_cnt = initial_permits_cnt;

    // public
    semaphore->try_acquire = _try_acquire;
    semaphore->acquire = _acquire;
#ifndef __ASYNC_API_DISABLE__
    semaphore->acquire_async = _acquire_async;
#endif

}
