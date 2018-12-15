/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Preemptive real-time process scheduler
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_SCHEDULER_H_
#define _SYS_SCHEDULER_H_

#include <stdbool.h>
#include <driver/vector.h>
#include <action.h>
#include <action/queue.h>
#include <defs.h>

// -------------------------------------------------------------------------------------

#define schedule_config(_schedule_config) ((Schedule_config_t *) (_schedule_config))

/**
 * Scheduler API return codes
 */
#define SCHEDULER_SUCCESS                                 KERNEL_API_SUCCESS
#define SCHEDULER_CONTEXT_SWITCH_HANDLE_EMPTY             signal(-1)
#define SCHEDULER_CONTEXT_SWITCH_HANDLE_UNSUPPORTED       signal(-2)

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
 * Insert process to runnable process queue, update priority according to given config
 *  - also initiate context switch if process has higher priority than running process
 *  - please note, that this function is only intended to be called from schedule_trigger or signal_trigger
 */
void schedule(Process_control_block_t *process, Schedule_config_t *config);

/**
 * Store signal to blocked_state_signal of action owner snd schedule it on trigger
 */
void schedule_trigger(Action_t *_this, signal_t signal);

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
 * - function signature corresponds to action_set_priority() for action type process
 */
void schedulable_state_reset(Process_control_block_t *process, priority_t priority_lowest);

/**
 * Switch running process execution state to 'waiting' for incoming signal
 *  - signal inserts itself to pending signals of process on trigger and if process is waiting for signal, then it is scheduled
 *  - process inherits priority if pending (unprocessed) signals
 *  - signal handler is executed and if handler returns false wait loop breaks and wait() returns
 */
signal_t wait(void);

/**
 * Remove process from runnable queue if suspend_condition set and insert to (optional) queue to wait for trigger
 *  - if given 'suspend_condition' is false, just apply with_config on running process and return 'non_blocking_return'
 *  - if blocking then return signal passed to process schedule trigger
 */
signal_t suspend(bool suspend_condition, Schedule_config_t *with_config, signal_t non_blocking_return, Action_queue_t *queue);

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
