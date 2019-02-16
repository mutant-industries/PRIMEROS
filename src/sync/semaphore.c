// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <sync/semaphore.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <process.h>


static signal_t _try_acquire(Semaphore_t *_this) {
    signal_t result = SEMAPHORE_SUCCESS;

    interrupt_suspend();

    if (semaphore_permits_cnt(_this)) {
        semaphore_permits_cnt(_this)--;
    }
    else {
        result = SEMAPHORE_NO_PERMITS;
    }

    interrupt_restore();

    return result;
}

static signal_t _acquire(Semaphore_t *_this, Time_unit_t *timeout, Schedule_config_t *with_config) {

    interrupt_suspend();
    // suspend running process if no permits are available, apply given config
    suspend(_try_acquire(_this), &_this->_queue, timeout, with_config);

    interrupt_restore();

    return running_process->blocked_state_signal;
}

static signal_t _acquire_async(Semaphore_t *_this, Action_t *action) {

    // sanity check
    if (action == action(running_process)) {
        return SEMAPHORE_INVALID_ARGUMENT;
    }

    interrupt_suspend();

    action_queue_insert(&_this->_queue, action);

    if (semaphore_permits_cnt(_this)) {
        // if there are permits, trigger action execution outside current context
        signal_trigger(action_signal(_this), action_signal_input(_this));
        // permits_count was incremented within default trigger, which needs rollback
        semaphore_permits_cnt(_this)--;
    }

    interrupt_restore();

    return SEMAPHORE_SUCCESS;
}

static bool _signal(Semaphore_t *_this, signal_t signal) {
    Action_t *action_to_signal;

    interrupt_suspend();

    // action queue might be empty already when signal handler reached
    if ((action_to_signal = action_queue_head(&_this->_queue))) {
        // only decrement permits count if some action is actually going to be triggered
        semaphore_permits_cnt(_this)--;
    }

    interrupt_restore();

    if (action_to_signal) {
        action_trigger(action_to_signal, signal);
    }

    // stay in waiting loop
    return true;
}

// -------------------------------------------------------------------------------------

static bool _on_semaphore_handled(Semaphore_t *_this) {
    // interrupts are disabled already

    return semaphore_permits_cnt(_this) != 0 && ! action_queue_is_empty(&_this->_queue);
}

static void _semaphore_trigger(Semaphore_t *_this, signal_t signal) {

    interrupt_suspend();

    if (action_queue_is_empty(&_this->_queue)) {
        // optimization - only trigger signal and context switch when it makes sense
        semaphore_permits_cnt(_this)++;
    }
    else {
        // the permits_count is also going to be incremented within default trigger
        signal_trigger(action_signal(_this), signal);
    }

    interrupt_restore();
}

// Semaphore_t destructor
static dispose_function_t _semaphore_dispose(Semaphore_t *_this) {

    _this->acquire = (signal_t (*)(Semaphore_t *, Time_unit_t *, Schedule_config_t *)) unsupported_after_disposed;
    _this->try_acquire = (signal_t (*)(Semaphore_t *)) unsupported_after_disposed;
    _this->acquire_async = (signal_t (*)(Semaphore_t *, Action_t *)) unsupported_after_disposed;

    action_queue_close(&_this->_queue, SEMAPHORE_DISPOSED);

    return NULL;
}

// Semaphore_t constructor
void semaphore_register(Semaphore_t *semaphore, uint16_t initial_permits_cnt, Process_control_block_t *context) {

    action_signal_create(semaphore, (dispose_function_t) _semaphore_dispose, _signal, NULL, context);
    // set default on_handled hook
    action_signal_on_handled(semaphore) = (bool (*)(Action_signal_t *)) _on_semaphore_handled;
    // semaphore does not inherit owner process priority by default
    sorted_set_item_priority(semaphore) = 0;
    // override signal trigger
    action(semaphore)->trigger = (action_trigger_t) _semaphore_trigger;
    // semaphore handler is defined, semaphore is never released from pending signals queue during handler
    action_on_released(semaphore) = NULL;
    // initialize semaphore queue
    action_queue_create(&semaphore->_queue, true, false, semaphore, action_default_set_priority);

    // state
    semaphore_permits_cnt(semaphore) = initial_permits_cnt;

    // public
    semaphore->try_acquire = _try_acquire;
    semaphore->acquire = _acquire;
    semaphore->acquire_async = _acquire_async;
}
