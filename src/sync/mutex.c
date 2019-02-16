// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <sync/mutex.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <process.h>


// -------------------------------------------------------------------------------------

#define _owner_attr arg_2
#define _mutex_owner(_mutex) action_attr(_mutex, _owner_attr)
#define _mutex_lockable(_mutex) (action(_mutex)->trigger == (action_trigger_t) action_default_release)

// -------------------------------------------------------------------------------------

static void _on_mutex_released(Mutex_t *_this) {
    // interrupts are disabled already

    // check whether mutex is being disposed
    if ( ! _mutex_lockable(_this)) {
        return;
    }

    // remove next waiting process if queue not empty, adjust mutex priority
    if (_mutex_owner(_this) = process(action_queue_pop(&_this->_queue))) {
        // enqueue mutex in owner process on_exit_action_queue, owner process inherit mutex priority
        action_queue_insert(&process(_mutex_owner(_this))->on_exit_action_queue, _this);
        // wakeup with requested config
        process_schedule(_mutex_owner(_this), MUTEX_SUCCESS);
        // locked already, reset nesting count
        _this->_nesting_cnt = 1;
    }
    else {
        // reset mutex state variables
        _this->_nesting_cnt = 0;
    }
}

// -------------------------------------------------------------------------------------

static signal_t _try_lock(Mutex_t *_this) {
    signal_t result = MUTEX_SUCCESS;

    interrupt_suspend();

    // check whether mutex is being disposed
    if ( ! _mutex_lockable(_this)) {
        result = MUTEX_DISPOSED;
    }
    else if ( ! _mutex_owner(_this)) {
        // mark owner
        _mutex_owner(_this) = running_process;
        // locked already, reset nesting count
        _this->_nesting_cnt = 1;
        // enqueue mutex in owner process on_exit_action_queue, owner process inherit mutex priority
        action_queue_insert(&process(_mutex_owner(_this))->on_exit_action_queue, _this);
    }
    else if (_mutex_owner(_this) == running_process) {
        _this->_nesting_cnt++;
    }
    else {
        result = MUTEX_LOCKED;
    }

    interrupt_restore();

    return result;
}

static signal_t _lock(Mutex_t *_this, Time_unit_t *timeout, Schedule_config_t *with_config) {

    interrupt_suspend();
    // suspend running process if mutex locked, apply given config
    suspend(_try_lock(_this), &_this->_queue, timeout, with_config);

    interrupt_restore();

    return running_process->blocked_state_signal;
}

static signal_t _unlock(Mutex_t *_this) {
    signal_t result = MUTEX_SUCCESS;

    interrupt_suspend();

    if (running_process != _mutex_owner(_this)) {
        // invalid owner or not locked
        result = MUTEX_INVALID_OWNER;
    }
    else if ( ! --_this->_nesting_cnt) {
        // reset mutex and wakeup next process if queue is not empty, initiate context switch
        action_release(_this);
    }

    interrupt_restore();

    return result;
}

// -------------------------------------------------------------------------------------

// Mutex_t destructor
static dispose_function_t _mutex_dispose(Mutex_t *_this) {

    // no more processes are going to be queued in mutex queue after this
    _this->lock = (signal_t (*)(Mutex_t *, Time_unit_t *, Schedule_config_t *)) unsupported_after_disposed;
    _this->try_lock = (signal_t (*)(Mutex_t *)) unsupported_after_disposed;
    _this->unlock = (signal_t (*)(Mutex_t *)) unsupported_after_disposed;

    // disable mutex inheriting queue head priority
    action_queue_on_head_priority_changed(&_this->_queue) = NULL;
    // release all waiting processes
    action_queue_close(&_this->_queue, MUTEX_DISPOSED);

    return NULL;
}

// Mutex_t constructor
void mutex_register(Mutex_t *mutex) {

    action_create(mutex, (dispose_function_t) _mutex_dispose, action_default_release);
    action_on_released(mutex) = (action_released_hook_t) _on_mutex_released;
    action_queue_create(&mutex->_queue, true, true, mutex, action_default_set_priority);
    _mutex_owner(mutex) = NULL;

    // state
    mutex->_nesting_cnt = 0;
    sorted_set_item_priority(mutex) = 0;

    // public
    mutex->try_lock = _try_lock;
    mutex->lock = _lock;
    mutex->unlock = _unlock;
}
