/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Schedule process on trigger
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_ACTION_SCHEDULE_H_
#define _SYS_ACTION_SCHEDULE_H_

#include <defs.h>
#include <action.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define action_schedule(_schedule) ((Action_schedule_t *) (_schedule))

/**
 * Action schedule public API access
 */
#define action_schedule_config(_schedule) (&(action_schedule(_schedule)->schedule_config))
#define _action_schedule_process_attr arg_1
#define action_schedule_process(_action) (action(_action)->_action_schedule_process_attr)

// -------------------------------------------------------------------------------------

/**
 * Action with schedule config and link to process that suppose to be scheduled
 */
typedef struct Action_schedule {
    // resource, schedule() on trigger
    Action_t _triggerable;
    // config for wakeup from blocking states
    Schedule_config_t schedule_config;

} Action_schedule_t;


void action_schedule_register(Action_schedule_t *action, dispose_function_t, Process_control_block_t *, Schedule_config_t *);

// -------------------------------------------------------------------------------------

/**
 * Store signal to 'blocked state return value' of linked process and reschedule it
 */
void schedule_executor(Action_schedule_t *_this, signal_t signal);


#endif /* _SYS_ACTION_SCHEDULE_H_ */
