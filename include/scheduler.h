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
    // priority to be set when scheduled
    priority_t priority;

} Schedule_config_t;

/**
 * Insert process to runnable process queue, update priority according to given config
 *  - also initiate context switch if process has higher priority than running process
 *  - please note, that this function is only intended to be called from {@see schedule_executor}
 */
void schedule(Process_control_block_t *process, Schedule_config_t *config);

/**
 *
 * Reset schedulable state and initiate context switch if another process becomes head of runnable queue
 * - reset schedule config on running process
 * - set priority to original priority or inherit priority of (possibly) owned mutex with highest priority
 * - initiate context switch if another process becomes head of runnable queue
 */
#define yield() schedulable_state_reset(running_process, true);

/**
 * Reset schedulable state and initiate context switch if another process becomes head of runnable queue
 * - if priority_lowest is set, {@see yield()}
 * - if not, then schedule config is not reset and if priority from config is higher that result priority,
 * then that one is used instead, also if result priority is same as current process priority, this function has no effect
 */
void schedulable_state_reset(Process_control_block_t *process, bool priority_lowest);

// -------------------------------------------------------------------------------------

/**
 * Set context switch handle during system start and initialize processing environment
 *  - can also be called anytime while system is running to change the handle
 */
signal_t scheduler_reinit(Context_switch_handle_t *handle, bool reset_state);


#endif /* _SYS_SCHEDULER_H_ */
