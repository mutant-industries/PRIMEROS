// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <scheduler.h>
#include <stddef.h>
#include <stdint.h>
#include <driver/cpu.h>
#include <driver/interrupt.h>
#include <driver/stack.h>
#include <action/signal.h>
#include <compiler.h>
#include <process.h>
#if defined(__PROCESS_LOCAL_WDT_CONFIG__) || defined(__SIGNAL_CLEAR_WDT_ON_HANDLED__)
#include <driver/wdt.h>
#endif

Process_control_block_t *running_process;

static Context_switch_handle_t *_context_switch_handle;
__persistent static Action_queue_t _runnable_queue = {0};

// -------------------------------------------------------------------------------------

void schedule(Process_control_block_t *process) {
    // assume interrupts are disabled already

    // sanity check
    if ( ! process_is_schedulable(process) || action_queue(deque_item_container(process)) == &_runnable_queue) {
        return;
    }

    // place process to runnable queue
    action_queue_insert(&_runnable_queue, process);

    // process is no longer in suspended state
    process_suspended(process) = false;

#ifndef __SIGNAL_PROCESSOR_DISABLE__
    // release schedule timeout if set
    action_release(&process->timed_schedule);
#endif

    context_switch_trigger();
}

bool schedule_handler(Action_t *_this, signal_t signal) {
    Process_control_block_t *process = process(action_owner(_this));

    interrupt_suspend();

    if (process_suspended(process)) {
        // if process was waiting then this handler was triggered from timed_schedule
        process_waiting(process) = false;
        // just store return value
        process->blocked_state_signal = signal;
        // and trigger scheduler responsible for placing process to correct queue
        schedule(process);
    }

    interrupt_restore();

    // stay in waiting loop (if used as handler)
    return true;
}

// -------------------------------------------------------------------------------------

void yield() {
    // reset process schedule config
    schedule_config_reset(process_schedule_config(running_process));
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
    if (process_schedule_config(process)->priority > new_priority) {
        new_priority = process_schedule_config(process)->priority;
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

signal_t wait(Time_unit_t *timeout, Schedule_config_t *with_config) {
    Time_unit_t wait_timeout;

    if (timeout) {
        // store timeout so that source structure can be modified within signal handlers
        time_unit_copy(timeout, &wait_timeout);
        // pass local copy to suspend
        timeout = &wait_timeout;
    }

    running_process->blocked_state_signal = SCHEDULER_SUCCESS;

    // make process schedulable via signal_trigger (if suspended)
    process_waiting(running_process) = true;
    // store / reset schedule config
    schedule_config_copy(with_config, process_schedule_config(running_process));
    // priority reset
    schedulable_state_reset(running_process, NULL);

    while (process_waiting(running_process)) {
        Action_signal_t *signal;

        // just peek, no pop - process still needs to inherit priority of signal during it's execution
        if ((signal = action_signal(action_queue_head(&running_process->pending_signal_queue)))) {

            // check whether priority should be locked
            if (action_signal_keep_priority_while_handled(signal)
                    && process_schedule_config(running_process)->priority < sorted_set_item_priority(signal)) {

                // disallow priority drop during signal handler if signal is released from pending queue
                process_schedule_config(running_process)->priority = sorted_set_item_priority(signal);
            }

            // execute action handler, process waiting state depends on handler return value
            process_waiting(running_process) = action(signal)->handler(action_owner(signal), action_signal_input(signal));

#ifdef __SIGNAL_CLEAR_WDT_ON_HANDLED__
            // clear WDT after signal handled
            WDT_clr();
#endif

            interrupt_suspend();

            // let the signal itself decide whether it should stay in pending queue (if still present)
            if (action_queue(deque_item_container(signal)) == &running_process->pending_signal_queue
                    && ( ! action_signal_on_handled(signal) || ! action_signal_handled_callback(signal))) {

                // release from pending queue, possible process priority update
                action_release(signal);
            }

            interrupt_restore();

            // store / reset schedule config
            schedule_config_copy(with_config, process_schedule_config(running_process));
            // priority reset
            schedulable_state_reset(running_process, NULL);
        }

        interrupt_suspend();

        // once again check whether process is still waiting - might be reset in schedule_handler (timeout)
        if (process_waiting(running_process) && action_queue_is_empty(&running_process->pending_signal_queue)) {
            suspend(TIMING_SIGNAL_TIMEOUT, NULL, timeout, with_config);
        }

        interrupt_restore();
    }

    return running_process->blocked_state_signal;
}

signal_t suspend(signal_t blocked_state_condition, Action_queue_t *queue, Time_unit_t *timeout, Schedule_config_t *with_config) {
    // assume interrupts are disabled already

    // store / reset schedule config
    schedule_config_copy(with_config, process_schedule_config(running_process));

    interrupt_suspend();

    running_process->blocked_state_signal = blocked_state_condition;

    if (blocked_state_condition != KERNEL_API_SUCCESS && blocked_state_condition != KERNEL_DISPOSED_RESOURCE_ACCESS) {

#ifndef __SIGNAL_PROCESSOR_DISABLE__
        // store process timed schedule delay if set
        if (timeout) {
            time_unit_copy(timeout, timed_signal_delay(&running_process->timed_schedule));
        }

        // try to set process timed schedule, check whether timing is initialized
        if (timeout && timed_signal_schedule(&running_process->timed_schedule) == TIMING_INVALID_STATE) {
            // invalid state, requested operation not possible
            running_process->blocked_state_signal = TIMING_INVALID_STATE;
        }
#else
        if (timeout) {
            // invalid state, requested operation not possible
            running_process->blocked_state_signal = TIMING_INVALID_STATE;
        }
#endif
        else {
            // remove running process from runnable queue
            action_release(running_process);
            // initiate context switch, priority reset
            schedulable_state_reset(running_process, NULL);
            // make process once schedulable via schedule_handler and signal_trigger
            process_suspended(running_process) = true;

            // standard enqueue if requested
            if (queue) {
                action_queue_insert(queue, running_process);
            }
        }
    }
    else if (with_config) {
        // possible priority shift if config set
        schedulable_state_reset(running_process, NULL);
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

#ifdef __PROCESS_LOCAL_WDT_CONFIG__
    // store current WDT state
    WDT_backup_to(&running_process->_WDT_state);
#endif

    if (running_process->_post_suspend_hook) {
        running_process->_post_suspend_hook(running_process);
    }

    running_process = process(action_queue_head(&_runnable_queue));

    if (running_process->_pre_schedule_hook) {
        running_process->_pre_schedule_hook(running_process);
    }

#ifdef __CONTEXT_SWITCH_HANDLE_CLEAR_IFG__
    vector_clear_interrupt_flag(_context_switch_handle);
#endif

#ifdef __PROCESS_LOCAL_WDT_CONFIG__
    // clear and restore process-local WDT state
    WDT_clr_restore_from(&running_process->_WDT_state);
#endif

    stack_restore_context(&running_process->_stack_pointer);

    reti;
}

// -------------------------------------------------------------------------------------

signal_t scheduler_reinit(Context_switch_handle_t *handle, bool persistent_state_reset) {

    // sanity check
    if ( ! handle) {
        return SCHEDULER_CONTEXT_SWITCH_HANDLE_EMPTY;
    }

    // try register vector interrupt service handler
    if (vector_register_raw_handler(handle, _context_switch, true)
            || vector_clear_interrupt_flag(handle) || vector_set_enabled(handle, true)) {

        return SCHEDULER_CONTEXT_SWITCH_HANDLE_UNSUPPORTED;
    }

    interrupt_suspend();

    if (persistent_state_reset) {
        // create runnable queue sorted by priority
        action_queue_create(&_runnable_queue, true);
    }

    // suspend current handle if set
    if (_context_switch_handle && _context_switch_handle != handle) {
        vector_set_enabled(_context_switch_handle, false);
    }

    _context_switch_handle = handle;

    interrupt_restore();

    return SCHEDULER_SUCCESS;
}
