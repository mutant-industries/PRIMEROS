// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <scheduler.h>
#include <stddef.h>
#include <stdint.h>
#include <driver/cpu.h>
#include <driver/interrupt.h>
#include <driver/stack.h>
#include <action/queue.h>
#include <defs.h>
#include <compiler.h>
#include <process.h>


Process_control_block_t *running_process;

static Context_switch_handle_t *_context_switch_handle;
// noinit to allow wakeup from deep sleep modes, where wakeup goes through C runtime startup code
__noinit static Action_queue_t _runnable_queue;

// -------------------------------------------------------------------------------------

void schedule(Process_control_block_t *process, Schedule_config_t *config) {
    priority_t new_priority;

    // sanity check
    if ( ! process_is_schedulable(process)) {
        return;
    }

    // nothing to be done
    if (action_queue(deque_item_container(process)) == &_runnable_queue
            && ( ! config || config->priority <= sorted_set_item_priority(process))) {
        return;
    }

    interrupt_suspend();

    if (action_queue(deque_item_container(process)) == &_runnable_queue) {
        // priority increase of already scheduled process
        action_set_priority(process, config->priority);
    }
    else {
        // process not scheduled yet (in no queue or in some other than runnable queue)
        new_priority = sorted_set_item_priority(process);

        // only priority increase allowed
        if (config && config->priority > new_priority) {
            new_priority = config->priority;
        }

        // remove process from whatever queue it is (possibly) linked to
        action_release(process);
        // adjust process priority
        sorted_set_item_priority(process) = new_priority;
        // and place to runnable queue
        action_queue_insert(&_runnable_queue, process);
    }

    context_switch_trigger();

    interrupt_restore();
}

void schedule_trigger(Action_t *_this, signal_t signal) {
    Process_control_block_t *process = process(action_owner(_this));

    // just store return value
    process->blocked_state_signal = signal;
    // and trigger scheduler responsible for placing process to correct queue
    schedule(process, process_get_schedule_config(process));
}

// -------------------------------------------------------------------------------------

void yield() {
    // reset process schedule config
    zerofill(process_get_schedule_config(running_process));
    // set lowest possible priority, place behind processes with the same priority
    schedulable_state_reset(running_process, PRIORITY_RESET);
}

void schedulable_state_reset(Process_control_block_t *process, priority_t priority_lowest) {

    // sanity check - kernel halt
    if (action_queue_is_empty(&_runnable_queue)) {
        return;
    }

    interrupt_suspend();

    priority_t new_priority = process->_original_priority;

    // inherit priority from parameter
    if (priority_lowest != PRIORITY_RESET && priority_lowest > new_priority) {
        new_priority = priority_lowest;
    }

    // inherit priority from schedule config if higher than original priority
    if (process_get_schedule_config(process)->priority > new_priority) {
        new_priority = process_get_schedule_config(process)->priority;
    }

    // inherit priority of exit action with highest priority
    if (action_queue_get_head_priority(&process->on_exit_action_queue) > new_priority) {
        new_priority = action_queue_get_head_priority(&process->on_exit_action_queue);
    }

    // inherit priority of pending signal with highest priority
    if (action_queue_get_head_priority(&process->pending_signal_queue) > new_priority) {
        new_priority = action_queue_get_head_priority(&process->pending_signal_queue);
    }

    // avoid process become last among processes with the same priority if not PRIORITY_RESET
    if (priority_lowest == PRIORITY_RESET || new_priority != sorted_set_item_priority(process)) {
        action_set_priority(process, new_priority);
    }

    context_switch_trigger();

    interrupt_restore();
}

signal_t wait() {

    running_process->blocked_state_signal = SCHEDULER_SUCCESS;

    process_waiting(running_process) = true;

    while (process_waiting(running_process)) {
        Action_t *pending_action;

        interrupt_suspend();

        if (action_queue_is_empty(&running_process->pending_signal_queue)) {
            // remove running process from runnable queue
            action_release(running_process);
            // initiate context switch, reset schedule config, priority reset
            yield();
        }

        interrupt_restore();

        // just peek, no pop - process still needs to inherit priority of signal during it's execution
        if ((pending_action = action_queue_head(&running_process->pending_signal_queue))) {
            // execute action handler
            process_waiting(running_process) = pending_action->handler(pending_action->arg_1, pending_action->arg_2);
            // release from pending_signal_queue, possible process priority update
            action_release(pending_action);
        }
    }

    return running_process->blocked_state_signal;
}

signal_t suspend(bool suspend_condition, Schedule_config_t *with_config, signal_t non_blocking_return, Action_queue_t *queue) {

    // store / reset schedule config
    bytecopy(with_config, process_get_schedule_config(running_process));

    interrupt_suspend();

    running_process->blocked_state_signal = non_blocking_return;

    if (suspend_condition) {
        // remove running process from runnable queue
        action_release(running_process);
        // initiate context switch, priority reset
        schedulable_state_reset(running_process, NULL);

        // standard enqueue if requested
        if (queue) {
            action_queue_insert(queue, running_process);
        }
    }
    else if (with_config) {
        // possible priority shift if config set
        process_schedule(running_process, non_blocking_return);
    }

    interrupt_restore();

    return running_process->blocked_state_signal;
}

// -------------------------------------------------------------------------------------

inline void context_switch_trigger() {

    // only initiate context switch if it makes sense
    if (running_process != process(action_queue_head(&_runnable_queue))) {
        vector_trigger(_context_switch_handle);
    }
}

__naked __interrupt void _context_switch() {

    stack_save_context(&running_process->_stack_pointer);
    running_process = process(action_queue_head(&_runnable_queue));
    stack_restore_context(&running_process->_stack_pointer);

    reti;
}

// -------------------------------------------------------------------------------------

signal_t scheduler_reinit(Context_switch_handle_t *handle, bool reset_state) {

    // sanity check
    if ( ! handle) {
        return SCHEDULER_CONTEXT_SWITCH_HANDLE_EMPTY;
    }

    // try register vector interrupt service handler
    if (vector_register_raw_handler(handle, _context_switch, true)
            || vector_clear_interrupt_flag(handle) || vector_set_enabled(handle, true)) {

        return SCHEDULER_CONTEXT_SWITCH_HANDLE_UNSUPPORTED;
    }

    // reset (possibly persistent) process queue
    if (reset_state) {
        // only sort queue by priority, trigger_all() is never executed on runnable queue (it does not make sense anyway)
        action_queue_create(&_runnable_queue, true, true);
    }

    // suspend current handle if set
    if (_context_switch_handle && _context_switch_handle != handle) {
        vector_set_enabled(_context_switch_handle, false);
    }

    _context_switch_handle = handle;

    return SCHEDULER_SUCCESS;
}
