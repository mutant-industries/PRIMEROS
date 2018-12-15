/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Process lifecycle
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_PROCESS_H_
#define _SYS_PROCESS_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <driver/cpu.h>
#include <action.h>
#include <action/queue.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define process(_process) ((Process_control_block_t *) (_process))

/**
 * Process public API access
 */
#define process_create(...) _PROCESS_CREATE_GET_MACRO(__VA_ARGS__, _process_create_2, _process_create_1)(__VA_ARGS__)
#define process_schedule(_process, _blocked_state_signal) action_trigger(_process, _blocked_state_signal)

//<editor-fold desc="variable-args - process_create()">
#define _PROCESS_CREATE_GET_MACRO(_1,_2,NAME,...) NAME
#define _process_create_1(_process) process_register(_process, NULL)
#define _process_create_2(_process, _create_config) process_register(_process, _create_config)
//</editor-fold>

// getter, setter
#define process_is_schedulable(_process) (action(_process)->trigger == (action_trigger_t) schedule_trigger)
#define process_is_alive(_process) process_is_schedulable(_process)
#define process_get_schedule_config(_process) (&(process(_process)->schedule_config))
#define process_get_original_priority(_process) (_process)->_original_priority
#define process_waiting(_process) (_process)->_waiting
#define process_local(_process) (_process)->_local
#define process_current_local() process_local(running_process)

/**
 * Process API return codes
 */
#define PROCESS_INVALID_ARGUMENT                        KERNEL_API_INVALID_ARGUMENT
#define PROCESS_SIGNAL_EXIT                             KERNEL_DISPOSED_RESOURCE_ACCESS

// -------------------------------------------------------------------------------------

/**
 * Process entry point function signature
 */
typedef signal_t (*process_entry_point_t)(signal_t arg_1, signal_t arg_2);

/**
 * Process init configuration
 */
typedef struct Process_create_config {
    // lowest address of memory allocated for stack - optional, use current stack if not set
    data_pointer_register_t stack_addr_low;
    // size of memory block allocated for stack (applies if stack_addr_low set)
    uint16_t stack_size;
    // initial process priority
    priority_t priority;
    // address to start execution at (applies if stack_addr_low set)
    process_entry_point_t entry_point;
    // argument 1 to be passed to process on start (applies if stack_addr_low set)
    signal_t arg_1;
    // argument 2 to be passed to process on start (applies if stack_addr_low set)
    signal_t arg_2;

} Process_create_config_t;

/**
 * Process state holder and control structure
 */
struct Process_control_block {
    // resource (process owner is parent process), schedule() on trigger
    Action_t _triggerable;
#ifdef __RESOURCE_MANAGEMENT_ENABLE__
    // list of resources owned by process
    Disposable_t *_resource_list;
#endif
    // top-of-stack of process
    data_pointer_register_t _stack_pointer;
    // set once on process start
    priority_t _original_priority;
    // exit code the process terminated with
    signal_t _exit_code;
    // process local storage
    void *_local;
    // process execution status - blocked waiting for signal
    bool _waiting;
    // recursive action priority update
    bool recursion_guard;
    // return value passing from blocking states
    signal_t blocked_state_signal;
    // process initialization parameters, allows process restart
    Process_create_config_t create_config;
    // config for wakeup from blocking states
    Schedule_config_t schedule_config;
    // queue of actions to be executed on process disposal sorted by action priority
    Action_queue_t on_exit_action_queue;
    // queue of actions waiting to be executed within
    Action_queue_t pending_signal_queue;

};

// -------------------------------------------------------------------------------------

/**
 * Create process with given configuration
 *  - configuration can be passed in (optional) parameter or can be preset on process.create_config
 *  - current process becomes owner of the new process if resource management is enabled
 */
void process_register(Process_control_block_t *process, Process_create_config_t *config);

/**
 * Return address of this function is initialized on process stack, shall be reached when process main function returns
 *  - also can be called any other time with the same effect
 *  - return value is passed as signal to all actions in process dispose action queue
 */
void process_exit(signal_t exit_code);

/**
 * Wait for given process to terminate, return actual return value of given process
 *  - reset priority according to given config
 *  - return PROCESS_SIGNAL_EXIT if process terminated already or if process was killed
 *  - wait for process to terminate and return it's exit code otherwise
 */
signal_t process_wait(Process_control_block_t *process, Schedule_config_t *with_config);

/**
 * non-blocking action enqueue - execute on target process termination, pass process exit code to action trigger
 *  - return false if process terminated already or if process was killed
 *  - please note, that process inherits priority of actions waiting for it's termination
 */
bool process_register_exit_action(Process_control_block_t *process, Action_t *action);

/**
 * Kill given process, release all allocated resources if resource management is enabled, wait for process to terminate
 */
void process_kill(Process_control_block_t *process);


#endif /* _SYS_PROCESS_H_ */
