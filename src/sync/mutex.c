// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <sync/mutex.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <process.h>


// -------------------------------------------------------------------------------------

#define _owner_attr arg_1
#define _owner(_mutex) action_attr(_mutex, _owner_attr)

// -------------------------------------------------------------------------------------

static void _release(Mutex_t *_this) {

    interrupt_suspend();
    // remove mutex from on_exit_action_queue on owner process, adjust process priority if inherited
    action_release(_this);

    // remove next waiting process if queue not empty, adjust mutex priority
    if (_owner(_this) = process(action_queue_pop(&_this->_queue))) {
        // enqueue mutex in owner process on_exit_action_queue, owner process inherit mutex priority
        action_queue_insert(&process(_owner(_this))->on_exit_action_queue, _this);
        // wakeup with requested config
        process_schedule(_owner(_this), MUTEX_SUCCESS);
        // locked already, reset nesting count
        _this->_nesting_cnt = 1;
    }
    else {
        // reset mutex state variables
        _this->_nesting_cnt = 0;
        sorted_set_item_priority(_this) = 0;
    }

    interrupt_restore();
}

// -------------------------------------------------------------------------------------

static signal_t _try_lock(Mutex_t *_this) {
    signal_t result = MUTEX_SUCCESS;

    interrupt_suspend();

    if ( ! _owner(_this)) {
        // mark owner
        _owner(_this) = running_process;
        // locked already, reset nesting count
        _this->_nesting_cnt = 1;
        // enqueue mutex in owner process on_exit_action_queue, owner process inherit mutex priority
        action_queue_insert(&process(_owner(_this))->on_exit_action_queue, _this);
    }
    else if (_owner(_this) == running_process) {
        _this->_nesting_cnt++;
    }
    else {
        result = MUTEX_LOCKED;
    }

    interrupt_restore();

    return result;
}

static signal_t _lock(Mutex_t *_this, Schedule_config_t *with_config) {

    interrupt_suspend();
    // suspend running process if mutex locked, apply given config
    suspend((bool) _try_lock(_this), with_config, MUTEX_SUCCESS, &_this->_queue);

    interrupt_restore();

    return running_process->blocked_state_signal;
}

static signal_t _unlock(Mutex_t *_this) {
    signal_t result = MUTEX_SUCCESS;

    interrupt_suspend();

    if (running_process != _owner(_this)) {
        // invalid owner or not locked
        result = MUTEX_INVALID_OWNER;
    }
    else if ( ! --_this->_nesting_cnt) {
        // reset mutex and wakeup next process if queue is not empty
        _release(_this);
        // initiate context switch, priority reset
        schedulable_state_reset(running_process, NULL);
    }

    interrupt_restore();

    return result;
}

// -------------------------------------------------------------------------------------

// Mutex_t destructor
static dispose_function_t _mutex_release(Mutex_t *_this) {

    // no more processes are going to be queued in mutex queue after this
    _this->lock = (signal_t (*)(Mutex_t *, Schedule_config_t *)) unsupported_after_disposed;
    _this->try_lock = (signal_t (*)(Mutex_t *)) unsupported_after_disposed;

    // disable mutex inheriting queue head priority
    action_queue_on_head_priority_changed(&_this->_queue) = NULL;
    // release all waiting processes
    action_queue_close(&_this->_queue, MUTEX_DISPOSED);

    interrupt_suspend();

    _this->unlock = (signal_t (*)(Mutex_t *)) unsupported_after_disposed;
    // release mutex owner and reset mutex state
    _release(_this);

    interrupt_restore();

    return NULL;
}

// Mutex_t constructor
void mutex_register(Mutex_t *mutex) {

    action_create(mutex, (dispose_function_t) _mutex_release, _release);
    action_queue_create(&mutex->_queue, true, true, mutex, action_default_set_priority);

    // state
    mutex->_nesting_cnt = 0;
    sorted_set_item_priority(mutex) = 0;

    // public
    mutex->try_lock = _try_lock;
    mutex->lock = _lock;
    mutex->unlock = _unlock;
}
