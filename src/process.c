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

        // child process disposal (no matter whether _this process terminated or was killed) - proper kill to stay consistent
        if (resource->_dispose_hook == _process_release) {
            process_kill(process(resource));
        }

        dispose(resource);
    }
#endif

    // disable priority inheritance from on_exit_action_queue
    action_queue_on_head_priority_changed(&_this->on_exit_action_queue) = NULL;
    // trigger all dispose actions registered on current process
    action_queue_close(&_this->on_exit_action_queue, _this->_exit_code);

    interrupt_suspend();

    // prevent the process from being scheduled
    action(_this)->trigger = (action_trigger_t) unsupported_after_disposed;

    // now is time to remove process from any (possible) queue
    action_release(_this);

    interrupt_restore();

    // only yield on process exit
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
    zerofill(&process->_triggerable);
    zerofill(process_get_schedule_config(process));
    process_get_schedule_config(process)->priority = create_config->priority;
    action_owner(process) = process;

    // reset process state
    process->_exit_code = PROCESS_SIGNAL_EXIT;
    process->_local = NULL;
    process->_waiting = false;
    process->recursion_guard = false;

    // reset action queues, inherit their priority
    action_queue_create(&process->on_exit_action_queue, true, true, process, schedulable_state_reset);
    action_queue_create(&process->pending_signal_queue, true, true, process, schedulable_state_reset);

    __dispose_hook_register(process, _process_release);

    // make process schedulable on trigger
    action(process)->trigger = (action_trigger_t) schedule_trigger;
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

    interrupt_suspend();
    // suspend running process if target is alive, apply given config
    suspend(process_is_alive(process), with_config, PROCESS_SIGNAL_EXIT, &process->on_exit_action_queue);

    interrupt_restore();

    return running_process->blocked_state_signal;
}

bool process_register_exit_action(Process_control_block_t *process, Action_t *action) {
    bool process_alive;

    interrupt_suspend();

    if ((process_alive = process_is_alive(process))) {
        // enqueue action to exit action queue on given process, sort by priority, let process inherit priority
        action_queue_insert(&process->on_exit_action_queue, action);
    }

    interrupt_restore();

    return process_alive;
}

void process_kill(Process_control_block_t *process) {

    // end process, release all related resources
    dispose(process);
    // process might be already executing dispose on itself with lower priority
    process_wait(process, process_get_schedule_config(running_process));
}
