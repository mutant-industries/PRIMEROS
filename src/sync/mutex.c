// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <sync/mutex.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <process.h>


// -------------------------------------------------------------------------------------

#define _owner_attr arg_1
#define _owner(_mutex) (action(_mutex)->_owner_attr)

// -------------------------------------------------------------------------------------

static void _release(Mutex_t *_this) {

    interrupt_suspend();
    // remove mutex from owned_mutex_queue on owner process
    remove(_this);

    // wakeup next waiting process if queue not empty
    if (_owner(_this) = process(action_queue_pop(&_this->_queue))) {
        // process no longer blocked on mutex
        process(_owner(_this))->blocked_on_mutex = NULL;
        // wakeup with requested config
        process_schedule(_owner(_this), MUTEX_SUCCESS);
        // locked already, reset nesting count
        _this->_nesting_cnt = 1;
        // set mutex priority to match priority of blocked process with highest priority or set to zero if queue is empty
        sorted_queue_item_priority(_this) = action_queue_empty(&_this->_queue)
                ? 0 : sorted_queue_item_priority(action_queue_head(&_this->_queue));
        // enqueue mutex in process owned_mutex_queue in case process terminates before mutex is released
        enqueue(_this, &process(_owner(_this))->owned_mutex_queue);
    }
    else {
        // reset mutex state variables
        _this->_nesting_cnt = 0;
        sorted_queue_item_priority(_this) = 0;
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
        // enqueue mutex in process owned_mutex_queue in case process terminates before mutex is released
        enqueue(_this, &running_process->owned_mutex_queue);
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

    // store / reset schedule config
    bytecopy(with_config, action_schedule_config(running_process));

    interrupt_suspend();

    running_process->blocked_state_signal = MUTEX_SUCCESS;

    if (_try_lock(_this)) {
        // remove running process from runnable queue
        remove(running_process);
        // initiate context switch, priority reset
        schedulable_state_reset(running_process, false);
        // link this mutex to process
        running_process->blocked_on_mutex = _this;

        // insert running process to mutex queue
        if (enqueue(running_process, &_this->_queue)) {
            // sort acquired mutexes by priority of process in waiting queue head
            action_set_priority(action(_this), sorted_queue_item_priority(running_process));
        }

        // inherit priority of owner if current process has higher
        if (sorted_queue_item_priority(running_process) > sorted_queue_item_priority(_owner(_this))) {
            action_set_priority(action(_owner(_this)), sorted_queue_item_priority(running_process));
        }
    }
    else {
        // possible priority shift for case when mutex is not locked
        process_schedule(running_process, MUTEX_SUCCESS);
    }

    interrupt_restore();

    return running_process->blocked_state_signal;
}

static void _lock_timeout(Mutex_t *_this, Process_control_block_t *process) {

    interrupt_suspend();

    // unlink this mutex from process
    process->blocked_on_mutex = NULL;

    // sanity check
    if (&_this->_queue == action_queue(deque_item_container(process))) {
        // check whether process is highest priority process in a queue
        bool highest_priority_process = action_queue_head(&_this->_queue) == action(process);

        // remove process from mutex queue
        remove(process);

        // only update mutex priority if it makes sense
        if (highest_priority_process && (action_queue_empty(&_this->_queue)
                 || sorted_queue_item_priority(action_queue_head(&_this->_queue)) != sorted_queue_item_priority(process))) {

            // set mutex priority to match priority of blocked process with highest priority or set to zero if queue is empty
            sorted_queue_item_priority(_this) = action_queue_empty(&_this->_queue)
                    ? 0 : sorted_queue_item_priority(action_queue_head(&_this->_queue));

            // and reset priority of mutex owner
            schedulable_state_reset(_owner(_this), false);
        }
    }

    interrupt_restore();
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
        schedulable_state_reset(running_process, false);
    }

    interrupt_restore();

    return result;
}

// -------------------------------------------------------------------------------------

// Semaphore_t destructor
static dispose_function_t _mutex_release(Mutex_t *_this) {
    Process_control_block_t *process_to_schedule = NULL;

    // no more processes are going to be queued in mutex queue after this
    _this->try_lock = (signal_t (*)(Mutex_t *)) unsupported_after_disposed;
    _this->lock = (signal_t (*)(Mutex_t *, Schedule_config_t *)) unsupported_after_disposed;
    _this->_lock_timeout = (void (*)(Mutex_t *, Process_control_block_t *)) unsupported_after_disposed;

    // release all waiting processes
    while (process_to_schedule = process(action_queue_pop(&_this->_queue))) {
        // no need to bother with priority of this mutex, (possible) mutex owner shall be released right away
        process_to_schedule->blocked_on_mutex = NULL;
        // let queued process know that mutex was disposed
        process_schedule(process_to_schedule, MUTEX_DISPOSED);
    }

    interrupt_suspend();

    _this->unlock = (signal_t (*)(Mutex_t *)) unsupported_after_disposed;
    // release mutex owner and reset mutex state
    _release(_this);

    interrupt_restore();

    return NULL;
}

// Mutex_t constructor
void mutex_register(Mutex_t *mutex) {

    // state
    action_register(action(mutex), (dispose_function_t) _mutex_release, (action_executor_t) _release, NULL, NULL, NULL);
    action_queue_init(&mutex->_queue, false);
    mutex->_nesting_cnt = 0;
    sorted_queue_item_priority(mutex) = 0;

    // protected
    mutex->_lock_timeout = _lock_timeout;

    // public
    mutex->try_lock = _try_lock;
    mutex->lock = _lock;
    mutex->unlock = _unlock;
}
