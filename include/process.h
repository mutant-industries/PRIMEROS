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
#include <time.h>

// -------------------------------------------------------------------------------------

#define process(_process) ((Process_control_block_t *) (_process))

/**
 * Process public API access
 */
#define process_create(...) _PROCESS_CREATE_GET_MACRO(__VA_ARGS__, _process_create_2, _process_create_1)(__VA_ARGS__)
#define process_schedule(_process, _blocked_state_signal) action_trigger(_process, _blocked_state_signal)
#define process_wait(...) _PROCESS_WAIT_GET_MACRO(__VA_ARGS__, _process_wait_3, _process_wait_2, _process_wait_1)(__VA_ARGS__)

//<editor-fold desc="variable-args - process_create()">
#define _PROCESS_CREATE_GET_MACRO(_1,_2,NAME,...) NAME
#define _process_create_1(_process) process_register(_process, NULL)
#define _process_create_2(_process, _create_config) process_register(_process, _create_config)
//</editor-fold>
//<editor-fold desc="variable-args - process_wait()">
#define _PROCESS_WAIT_GET_MACRO(_1,_2,_3,NAME,...) NAME
#define _process_wait_1(_process) process_wait_for(_process, NULL, NULL)
#define _process_wait_2(_process, _timeout) process_wait_for(_process, _timeout, NULL)
#define _process_wait_3(_process, _timeout, _with_config) process_wait_for(_process, _timeout, _with_config)
//</editor-fold>

// getter, setter
#define process_is_schedulable(_process) (action(_process)->trigger == (action_trigger_t) schedule_handler)
#define process_is_alive(_process) process_is_schedulable(_process)
#define process_schedule_config(_process) (&(process(_process)->_schedule_config))
#define process_get_original_priority(_process) (_process)->_original_priority
#define process_suspended(_process) (_process)->_suspended
#define process_waiting(_process) (_process)->_waiting
#define process_exit_code(_process) (_process)->_exit_code
#define process_local(_process) (_process)->_local
#define process_current_local() process_local(running_process)
#define process_pre_schedule_hook(_process) (_process)->_pre_schedule_hook
#define process_current_pre_schedule_hook() process_pre_schedule_hook(running_process)
#define process_post_suspend_hook(_process) (_process)->_post_suspend_hook
#define process_current_post_suspend_hook() process_post_suspend_hook(running_process)

/**
 * Process API return codes
 */
#define PROCESS_INVALID_ARGUMENT        KERNEL_API_INVALID_ARGUMENT
#define PROCESS_SIGNAL_EXIT             KERNEL_DISPOSED_RESOURCE_ACCESS
#define PROCESS_WAIT_TIMEOUT            KERNEL_API_TIMEOUT

#ifndef __SIGNAL_PROCESSOR_DISABLE__

// usleep(usecs), usleep(secs, usecs)
#define usleep(...) __suspend_timed__(timeout_usecs(__VA_ARGS__))
// millisleep(millisecs), millisleep(secs, millisecs)
#define millisleep(...) __suspend_timed__(timeout_millisecs(__VA_ARGS__))
// sleep(secs), sleep(secs, millisecs, usecs)
#define sleep(...) __suspend_timed__(timeout_secs(__VA_ARGS__))
// sleep_long(hrs), sleep_long(hrs, secs), sleep_long(hrs, secs, millisecs), sleep_long(hrs, secs, millisecs, usecs)
#define sleep_long(...) __suspend_timed__(timeout_hrs(__VA_ARGS__))
// sleep_cached() - reuse last used sleep interval, pre-cache possible by calling timeout_[interval](...)
#define sleep_cached() __suspend_timed__(timeout_cached())
// default timed suspend entry point
#define __suspend_timed__(_time_unit) suspend(TIMING_SIGNAL_TIMEOUT, NULL, _time_unit, process_schedule_config(&running_process))

/**
 * Delay setters on &running_process->timed_schedule
 * - typical usage within blocking API (blocking wait with timeout), e.g. process_wait(process, timeout_secs(1))
 */
// (usecs) -> delay, (secs, usecs) -> delay
#define timeout_usecs(...) timed_signal_set_delay_usecs(&running_process->timed_schedule, __VA_ARGS__)
// (millisecs) -> delay, (secs, millisecs) -> delay
#define timeout_millisecs(...) timed_signal_set_delay_millisecs(&running_process->timed_schedule, __VA_ARGS__)
// (secs) -> delay, (secs, millisecs, usecs) -> delay
#define timeout_secs(...) timed_signal_set_delay_secs(&running_process->timed_schedule, __VA_ARGS__)
// (hrs) -> delay, (hrs, secs) -> delay, (hrs, secs, millisecs) -> delay, (hrs, secs, millisecs, usecs) -> delay
#define timeout_hrs(...) timed_signal_set_delay_hrs(&running_process->timed_schedule, __VA_ARGS__)
// () -> delay - reuse previously set delay
#define timeout_cached() timed_signal_delay(&running_process->timed_schedule)

#endif /* __SIGNAL_PROCESSOR_DISABLE__ */

// -------------------------------------------------------------------------------------

/**
 * Process entry point function signature
 */
typedef signal_t (*process_entry_point_t)(signal_t arg_1, signal_t arg_2);

/**
 * Scheduler hook signature
 *  - can be registered on context switched to / from process
 *  - executed within interrupt service
 */
typedef void (*process_schedule_hook_t)(Process_control_block_t *);

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
    // set if not present in runnable queue, schedule synchronization flag
    bool _suspended;
    // execution state blocked waiting for signal(s)
    bool _waiting;
    // hook triggered before context switched to this process
    process_schedule_hook_t _pre_schedule_hook;
    // hook triggered after process is suspended
    process_schedule_hook_t _post_suspend_hook;
#ifdef __PROCESS_LOCAL_WDT_CONFIG__
    // watchdog configuration for this process
    uint16_t _WDT_state;
#endif
    // return value passing from blocking states
    signal_t blocked_state_signal;
    // process initialization parameters, allows process restart
    Process_create_config_t create_config;
    // config for wakeup from blocking states
    Schedule_config_t _schedule_config;
    // queue of actions to be executed on process disposal
    Action_queue_t on_exit_action_queue;
    // queue of actions of type signal waiting to be executed within waiting state
    Action_queue_t pending_signal_queue;
#ifndef __SIGNAL_PROCESSOR_DISABLE__
    // signal used to wakeup process from blocking states (blocking wait with timeout)
    Timed_signal_t timed_schedule;
#endif
};

// -------------------------------------------------------------------------------------

/**
 * Create process with given configuration
 *  - configuration can be passed in (optional) parameter or can be preset on process.create_config
 *  - current process becomes owner of the new process if resource management is enabled
 *  - process is created in schedulable state however it is not scheduled
 */
void process_register(Process_control_block_t *process, Process_create_config_t *config);

/**
 * Return address of this function is initialized on process stack, shall be reached when process main function returns
 *  - also can be called any other time with the same effect
 *  - return value is passed as signal to all actions in process exit action queue
 */
void process_exit(signal_t exit_code);

/**
 * Wait for given process to terminate, return actual return value of given process
 *  - reset priority according to given config
 *  - return PROCESS_SIGNAL_EXIT if process terminated already or if process was killed
 *  - wait for process to terminate and return it's exit code otherwise
 */
signal_t process_wait_for(Process_control_block_t *process, Time_unit_t *timeout, Schedule_config_t *with_config);

/**
 * non-blocking action enqueue - execute on target process termination, pass process exit code to action trigger
 *  - return false if process terminated already or if process was killed
 *  - please note, that process inherits priority of actions waiting for it's termination
 */
bool process_wait_for_async(Process_control_block_t *process, Action_t *action);

/**
 * Kill given process, release all it's allocated resources if resource management is enabled
 */
void process_kill(Process_control_block_t *process);


#endif /* _SYS_PROCESS_H_ */
