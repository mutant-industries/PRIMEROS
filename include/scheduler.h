/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Preemptive real-time process scheduler, sleep, blocking wait with timeout
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_SCHEDULER_H_
#define _SYS_SCHEDULER_H_

#include <stdbool.h>
#include <driver/vector.h>
#include <defs.h>
#include <action.h>
#include <action/queue.h>

// -------------------------------------------------------------------------------------

// signal_wait(), signal_wait(timeout), signal_wait(timeout, schedule_config)
#define signal_wait(...) _SIGNAL_WAIT_GET_MACRO(_0, ##__VA_ARGS__, _signal_wait_2, _signal_wait_1, _signal_wait_0)(__VA_ARGS__)

//<editor-fold desc="variable-args - signal_wait()">
#define _SIGNAL_WAIT_GET_MACRO(_0,_1,_2,NAME,...) NAME
#define _signal_wait_0() wait(NULL, NULL)
#define _signal_wait_1(_timeout) wait(_timeout, NULL)
#define _signal_wait_2(_timeout, _with_config) wait(_timeout, _with_config)
//</editor-fold>

/**
 * Scheduler API return codes
 */
#define SCHEDULER_SUCCESS                                 KERNEL_API_SUCCESS
#define SCHEDULER_CONTEXT_SWITCH_HANDLE_EMPTY             signal(-1)
#define SCHEDULER_CONTEXT_SWITCH_HANDLE_UNSUPPORTED       signal(-2)

// duplicate source schedule config to target, reset target if source empty
#define schedule_config_copy(source, target) ((target)->priority = (source) ? (source)->priority : 0)
#define schedule_config_reset(schedule_config) (schedule_config)->priority = 0

// -------------------------------------------------------------------------------------

#define Context_switch_handle_t Vector_handle_t

/**
 * Process schedule configuration
 */
typedef struct Schedule_config {
    // lowest priority to be set when scheduled
    priority_t priority;

} Schedule_config_t;

/**
 * Insert process to runnable process queue
 *  - initiate context switch if process has higher priority than running process
 *  - no thread safety, this function is only intended to be called from schedule_handler or signal_trigger
 */
void schedule(Process_control_block_t *process);

/**
 * Schedule owner of given action (if suspended) and pass given signal to it's blocked_state_signal
 *  - action owner must be a process
 *  - might be used as both - action handler or action trigger
 */
bool schedule_handler(Action_t *_this, signal_t signal);

/**
 * Reset schedulable state and initiate context switch if another process becomes head of runnable queue
 *  - reset schedule config on running process
 *  - set priority to original or inherit priority - for priority inheritance {@see schedulable_state_reset}
 *  - place process behind all processes with the same priority
 *  - initiate context switch if another process becomes head of runnable queue
 */
void yield(void);

/**
 * Reset schedulable state and initiate context switch if another process becomes head of runnable queue
 *  - set priority to max(original process priority, priority_lowest, priority from schedule config)
 *  - set priority at least to queue head priority of 'exit actions queue' (mutexes, actions waiting for process termination)
 *  - set priority at least to queue head priority of 'pending signals queue'
 *  - if result priority equals current process priority and priority_lowest == PRIORITY_RESET, then
 * place process behind all processes with the same priority
 *  - function signature corresponds to action_set_priority() for action type process
 */
void schedulable_state_reset(Process_control_block_t *process, priority_t priority_lowest);

/**
 * Switch running process execution state to 'waiting' for incoming signal
 *  - signal inserts itself to pending signals of process on trigger and if process is waiting for signal, then it is scheduled
 *  - process inherits priority of pending signal queue
 *  - signal handler is executed and if handler returns false wait loop breaks and wait() returns
 *  - schedule config determines lowest priority of process to be applied until next blocking call / yield
 *  - return SCHEDULER_SUCCESS or TIMING_SIGNAL_TIMEOUT on timeout
 */
signal_t wait(Time_unit_t *timeout, Schedule_config_t *with_config);

/**
 * Remove process from runnable queue if blocked_state_condition set and insert to (optional) queue to wait for trigger
 *  - if blocking then return signal passed to process schedule trigger
 *  - on timeout return TIMING_SIGNAL_TIMEOUT
 */
signal_t suspend(signal_t blocked_state_condition, Action_queue_t *queue, Time_unit_t *timeout, Schedule_config_t *with_config);

// -------------------------------------------------------------------------------------

/**
 * Initiate context switch
 *  - context switch is actually triggered only if running process is not head of runnable queue
 */
void context_switch_trigger(void);

/**
 * Set context switch handle during system start and initialize processing environment
 *  - can also be called anytime while system is running to change the handle
 */
signal_t scheduler_reinit(Context_switch_handle_t *handle, bool reset_state);


#endif /* _SYS_SCHEDULER_H_ */
