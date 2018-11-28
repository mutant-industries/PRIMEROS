// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <process.h>
#include <driver/interrupt.h>
#include <driver/stack.h>
#include <compiler.h>


Process_control_block_t *running_process;

static Context_switch_handle_t *_context_switch_handle;
// noinit to allow wakeup from deep sleep modes, where wakeup goes through C runtime startup code
__noinit static Process_control_block_t *_runnable_queue;

void process_context_switch(void);

// -------------------------------------------------------------------------------------

// Process_control_block_t destructor
static dispose_function_t _process_release(Process_control_block_t *_this) {

    // execute exit hook if set
    if (_this->exit_hook) {
        _this->exit_hook(_this->_exit_code);
    }

#ifdef __RESOURCE_MANAGEMENT_ENABLE__
    // release all resources that belong to current process
    while (_this->_resource_list) {
        Disposable_t *resource = _this->_resource_list;

        // child process disposal (no matter whether _this process terminated or was killed) - proper kill to avoid deadlock
        if (resource->_dispose_hook == _process_release) {
            process_kill((Process_control_block_t *) resource);
        }

        dispose(resource);
    }
#endif

    interrupt_suspend();

    // prevent the process from being scheduled
    _this->alive = false;

    // execute all dispose actions registered on current process
    action_execute_all(_this->_dispose_action_queue, _this->_exit_code);

    // now is time to remove process from runnable queue if present
    action_dequeue((Action_t *) _this);

    // only yield on process exit, also no yield on init process dispose (kernel halt)
    if (_this == running_process && _runnable_queue) {
        yield();    // ... and never return
    }

    interrupt_restore();

    return NULL;
}

// Process_control_block_t constructor
void process_create(Process_control_block_t *process, Process_create_config_t *config) {

#ifdef __RESOURCE_MANAGEMENT_ENABLE__
    // reset resource list
    process->_resource_list = NULL;
#endif

    // initialize stack if requested, use current stack otherwise
    if (config->stack_addr_low) {
        deferred_stack_pointer_init(&process->_stack_pointer, config->stack_addr_low, config->stack_size);
        deferred_stack_push_return_address(&process->_stack_pointer, process_exit);
        deferred_stack_context_init(&process->_stack_pointer, config->entry_point, config->argument);
    }

    // reset process priority
    process->_original_priority = config->priority;
    process->last_set_priority = config->priority;
    process->_schedulable.priority = config->priority;

    // prepare general schedule action on wakeup from blocking states
    process->_schedulable.execute = process_reschedule_executor;
    process->_schedulable.handler = NULL;
    process->_schedulable.arg_1 = process;
    process->_schedulable.arg_2 = &process->schedule_config;
    process->_schedulable.queue = NULL;

    // reset queue of actions to be executed on process disposal
    process->_dispose_action_queue = NULL;

    // exit code is only set on proper process termination
    process->_exit_code = PROCESS_KILLED;

    // set exit hook and scheduler hooks
    process->exit_hook = config->exit_hook;
    process->pre_schedule_hook = config->pre_schedule_hook;
    process->post_suspend_hook = config->post_suspend_hook;

    __dispose_hook_register(process, _process_release);

    // make process ready to be scheduled
    process->alive = true;

    // ... schedule it and initiate context switch eventually
    process_schedule(process, NULL);
}

void process_exit(int16_t exit_code) {

    // store exit code to be passed to exit hook and actions in dispose action queue
    running_process->_exit_code = exit_code;
    // end process, release all related resources
    dispose(running_process);
}

// -------------------------------------------------------------------------------------

#ifndef __ASYNC_API_DISABLE__

bool process_register_dispose_action(Process_control_block_t *process, Action_t *action) {
    bool process_alive;

    interrupt_suspend();

    if ((process_alive = process->alive)) {
        // add action on dispose list of given process, sort by priority
        action_enqueue(action, &process->_dispose_action_queue);
    }

    interrupt_restore();

    return process_alive;
}

#endif

int16_t process_wait(Process_control_block_t *process, Process_schedule_config_t *config) {

    // sanity check
    if (process == running_process) {
        return PROCESS_INVALID_ARGUMENT;
    }

    // possible priority shift for case when target process is not running already
    process_schedule(running_process, config);

    running_process->blocked_state_return_value = KERNEL_DISPOSED_RESOURCE_ACCESS;

    interrupt_suspend();

    if (process->alive) {
        // store / reset schedule config
        bytecopy(config, &process->schedule_config);
        // suspend running process until target process terminates
        process_current_suspend();
        // register running process reschedule action on target process
        action_enqueue((Action_t *) running_process, &process->_dispose_action_queue);
    }

    interrupt_restore();

    return running_process->blocked_state_return_value;
}

void process_kill(Process_control_block_t *process) {

    // end process, release all related resources
    dispose(process);
    // process might be already executing dispose on itself with lower priority
    process_wait(process, NULL);
}

void process_suspend(Process_control_block_t *process) {

    // sanity check
    if (process->_schedulable.queue != (Action_t **) &_runnable_queue) {
        return;
    }

    // remove process from runnable queue
    action_dequeue((Action_t *) process);

    // initiate context switch
    if (process == running_process) {
        yield();
    }
}

// -------------------------------------------------------------------------------------

void process_schedule(Process_control_block_t *process, Process_schedule_config_t *config) {
    bool reschedule_required = false;

    // sanity check
    if ( ! process->alive) {
        return;
    }

    // nothing to be done
    if (process->_schedulable.queue == (Action_t **) &_runnable_queue && ! config) {
        return;
    }

    interrupt_suspend();

    if (config && config->priority > process->_schedulable.priority) {
        process->_schedulable.priority = process->last_set_priority = config->priority;
    }

    if ( ! process->_schedulable.queue) {
        // request enqueue to runnable queue
        reschedule_required = true;
    }
    else if (process != running_process && process->_schedulable.queue) {
        // remove process from whatever queue it is linked to
        action_dequeue((Action_t *) process);
        // request enqueue to runnable queue
        reschedule_required = true;
    }

    interrupt_restore();

    if ( ! reschedule_required) {
        return;
    }

    if (action_enqueue((Action_t *) process, (Action_t **) &_runnable_queue)) {
        ((Vector_handle_t *) _context_switch_handle)->trigger((Vector_handle_t *) _context_switch_handle);
    }
}

void process_reschedule_executor(Action_t *_this, int16_t signal) {

    // just store return value
    ((Process_control_block_t *) _this->arg_1)->blocked_state_return_value = signal;
    // and execute scheduler, which itself is responsible for placing process to correct queue
    process_schedule((Process_control_block_t *) _this->arg_1, (Process_schedule_config_t *) _this->arg_2);
}


void yield() {

    interrupt_suspend();

    // reset priority
    if (running_process->_schedulable.priority > running_process->_original_priority) {
        running_process->_schedulable.priority = running_process->last_set_priority = running_process->_original_priority;

        if (running_process->_schedulable.queue) {
            // remove process from original queue since it might not be sorted anymore
            list_remove((List_item_t *) running_process, (List_item_t **) running_process->_schedulable.queue);
            // reinsert and sort
            action_enqueue((Action_t *) running_process, running_process->_schedulable.queue);
        }
    }

    interrupt_restore();

    // only initiate context switch if it makes sense
    if (running_process != _runnable_queue) {
        ((Vector_handle_t *) _context_switch_handle)->trigger((Vector_handle_t *) _context_switch_handle);
    }
}

// -------------------------------------------------------------------------------------

__naked __interrupt void process_context_switch() {

    stack_save_context(&running_process->_stack_pointer);

    if (running_process->post_suspend_hook) {
        running_process->post_suspend_hook();
    }

    running_process = _runnable_queue;

    if (running_process->pre_schedule_hook) {
        running_process->pre_schedule_hook();
    }

    stack_restore_context(&running_process->_stack_pointer);

    reti;
}

// -------------------------------------------------------------------------------------

int16_t processing_reinit(Context_switch_handle_t *handle, bool reset_state) {
    Vector_handle_t *vector = (Vector_handle_t *) handle;

    // sanity check
    if ( ! handle) {
        return PROCESS_CONTEXT_SWITCH_HANDLE_EMPTY;
    }

    // try register handle interrupt service handler
    if (vector->register_raw_handler(vector, process_context_switch, true)
            || vector->clear_interrupt_flag(vector) || vector->set_enabled(vector, true)) {

        return PROCESS_CONTEXT_SWITCH_HANDLE_UNSUPPORTED;
    }

    // reset (possibly persistent) process queue
    if (reset_state) {
        _runnable_queue = NULL;
    }

    // suspend current handle if set
    if (_context_switch_handle && _context_switch_handle != handle) {
        ((Vector_handle_t *) _context_switch_handle)->set_enabled(((Vector_handle_t *) _context_switch_handle), false);
    }

    _context_switch_handle = handle;

    return PROCESS_SUCCESS;
}
