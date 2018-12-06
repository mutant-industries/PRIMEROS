// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <scheduler.h>
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
    if ( ! process->alive) {
        return;
    }

    // nothing to be done
    if (action_queue(deque_item_container(process)) == &_runnable_queue
            && ( ! config || config->priority <= sorted_queue_item_priority(process))) {
        return;
    }

    interrupt_suspend();

    bool context_switch_required;

    if (action_queue(deque_item_container(process)) == &_runnable_queue) {
        // priority increase of already scheduled process
        context_switch_required = set_priority(process, config->priority) && process != running_process;
    }
    else {
        // process not scheduled yet (in no queue or in some other than runnable queue)
        new_priority = sorted_queue_item_priority(process);

        if (config && config->priority > new_priority) {
            // only priority increase allowed
            new_priority = config->priority;
        }

        // remove process from whatever queue it is (possibly) linked to
        action_remove(action(process));
        // adjust process priority
        sorted_queue_item_priority(process) = new_priority;
        // and place to runnable queue
        context_switch_required = enqueue(process, &_runnable_queue);
    }

    interrupt_restore();

    if (context_switch_required) {
        vector_trigger(_context_switch_handle);
    }
}

// -------------------------------------------------------------------------------------

void schedulable_state_reset(Process_control_block_t *process, bool priority_lowest) {

    // sanity check - kernel halt
    if (action_queue_empty(&_runnable_queue)) {
        return;
    }

    interrupt_suspend();

    priority_t new_priority = process->_original_priority;

    if (priority_lowest) {
        // reset schedule config
        zerofill(action_schedule_config(process));
    }
    else if (action_schedule_config(process)->priority > new_priority) {
        // inherit priority from schedule config if higher than original priority
        new_priority = action_schedule_config(process)->priority;
    }

    // always inherit priority of owned mutex with highest priority
    if ( ! action_queue_empty(&running_process->owned_mutex_queue) &&
            sorted_queue_item_priority(action_queue_head(&running_process->owned_mutex_queue)) > new_priority) {

        new_priority = sorted_queue_item_priority(action_queue_head(&running_process->owned_mutex_queue));
    }

    // avoid process become last among processes with the same priority
    if (priority_lowest || new_priority != sorted_queue_item_priority(process)) {
        set_priority(running_process, new_priority);
    }

    interrupt_restore();

    // only initiate context switch if it makes sense
    if (running_process != process(action_queue_head(&_runnable_queue))) {
        vector_trigger(_context_switch_handle);
    }
}

// -------------------------------------------------------------------------------------

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
        action_queue_init(&_runnable_queue, true);
    }

    // suspend current handle if set
    if (_context_switch_handle && _context_switch_handle != handle) {
        vector_set_enabled(_context_switch_handle, false);
    }

    _context_switch_handle = handle;

    return SCHEDULER_SUCCESS;
}
