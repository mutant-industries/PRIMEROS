// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <process.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <driver/stack.h>


// Process_control_block_t destructor
static dispose_function_t _process_release(Process_control_block_t *_this) {

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

    // process kill, notify (possible) mutex process was blocked on if leaving mutex queue
    if (_this != running_process && _this->blocked_on_mutex) {
        _this->blocked_on_mutex->_lock_timeout(_this->blocked_on_mutex, _this);
    }

    interrupt_suspend();

    // prevent the process from being scheduled
    _this->alive = false;

    // trigger all dispose actions registered on current process
    action_queue_trigger_all(&_this->_on_dispose_action_queue, _this->_exit_code);
    // release all owned locks
    action_queue_trigger_all(&_this->owned_mutex_queue, NULL);

    // now is time to remove process from any (possible) queue
    action_remove(action(_this));

    interrupt_restore();

    // only yield on process exit, also no yield on init process dispose (kernel halt)
    if (_this == running_process) {
        yield();    // ... and never return
    }

    return NULL;
}

// Process_control_block_t constructor
void process_register(Process_control_block_t *process, Process_create_config_t *config) {

#ifdef __RESOURCE_MANAGEMENT_ENABLE__
    // reset resource list
    process->_resource_list = NULL;
#endif

    Process_create_config_t *create_config = &process->create_config;

    if (config) {
        bytecopy(config, create_config);
    }

    // initialize stack if requested, use current stack otherwise
    if (create_config->stack_addr_low) {
        deferred_stack_pointer_init(&process->_stack_pointer, create_config->stack_addr_low, create_config->stack_size);
        deferred_stack_push_return_address(&process->_stack_pointer, process_exit);
        deferred_stack_context_init(&process->_stack_pointer, create_config->entry_point, create_config->arg_1, create_config->arg_2);
    }

    // reset process priority
    process->_original_priority = create_config->priority;

    // prepare general schedule action on wakeup from blocking states
    action(process)->trigger = (action_executor_t) schedule_executor;
    action_schedule_process(process) = process;
    zerofill(action_schedule_config(process));
    action_schedule_config(process)->priority = create_config->priority;

    // reset action queues
    action_queue_init(&process->_on_dispose_action_queue, false);
    action_queue_init(&process->owned_mutex_queue, false);
    // reset (possible) mutex reference
    process->blocked_on_mutex = NULL;

    // exit code is only set on proper process termination
    process->_exit_code = PROCESS_SIGNAL_EXIT;

    __dispose_hook_register(process, _process_release);

    // make process ready to be scheduled
    process->alive = true;
}

void process_exit(signal_t exit_code) {

    // store exit code to be passed to exit hook and actions in dispose action queue
    running_process->_exit_code = exit_code;
    // end process, release all related resources
    dispose(running_process);
}

// -------------------------------------------------------------------------------------

signal_t process_wait(Process_control_block_t *process, Schedule_config_t *with_config) {

    // sanity check
    if (process == running_process) {
        return PROCESS_INVALID_ARGUMENT;
    }

    // store / reset schedule config
    bytecopy(with_config, action_schedule_config(running_process));

    interrupt_suspend();

    running_process->blocked_state_signal = PROCESS_SIGNAL_EXIT;

    if (process->alive) {
        // remove running process from runnable queue
        remove(running_process);
        // initiate context switch, priority reset
        schedulable_state_reset(running_process, false);
        // register running process reschedule action on target process
        enqueue(running_process, &process->_on_dispose_action_queue);
    }
    else {
        // possible priority shift for case when process is not alive
        process_schedule(running_process, PROCESS_SIGNAL_EXIT);
    }

    interrupt_restore();

    return running_process->blocked_state_signal;
}

#ifndef __ASYNC_API_DISABLE__

bool process_register_dispose_action(Process_control_block_t *process, Action_t *action) {
    bool process_alive;

    interrupt_suspend();

    if ((process_alive = process->alive)) {
        // add action on dispose list of given process, sort by priority
        enqueue(action, &process->_on_dispose_action_queue);
    }

    interrupt_restore();

    return process_alive;
}

#endif

void process_kill(Process_control_block_t *process) {

    // end process, release all related resources
    dispose(process);
    // process might be already executing dispose on itself with lower priority
    process_wait(process, action_schedule_config(running_process));
}
