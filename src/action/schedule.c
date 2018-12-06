// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action/schedule.h>
#include <stddef.h>
#include <driver/interrupt.h>
#include <process.h>
#include <sync/mutex.h>


void schedule_executor(Action_schedule_t *_this, signal_t signal) {
    Process_control_block_t *process_to_schedule = action_schedule_process(_this);
    Mutex_t *blocked_on_mutex;

    // notify (possible) mutex process is blocked on
    if ((blocked_on_mutex = process_to_schedule->blocked_on_mutex)) {
        blocked_on_mutex->_lock_timeout(blocked_on_mutex, process_to_schedule);
    }

    // just store return value
    process_to_schedule->blocked_state_signal = signal;
    // and trigger scheduler, which itself is responsible for placing process to correct queue
    schedule(process_to_schedule, &_this->schedule_config);
}

// -------------------------------------------------------------------------------------

// Action_schedule_t constructor
void action_schedule_register(Action_schedule_t *action, dispose_function_t dispose_hook, Process_control_block_t *process,
                                    Schedule_config_t *schedule_config) {

    // call parent constructor
    action_register((Action_t *) action, dispose_hook, (action_executor_t) schedule_executor, NULL, process, NULL);
    // store / reset schedule config
    bytecopy(schedule_config, &action->schedule_config)
}
