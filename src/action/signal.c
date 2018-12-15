// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action/signal.h>
#include <action/queue.h>
#include <driver/interrupt.h>
#include <process.h>


// Action_signal_t constructor
void action_signal_register(Action_signal_t *signal, dispose_function_t dispose_hook, Process_control_block_t *context,
        action_handler_t handler, Schedule_config_t *with_config) {

    action_create(signal, dispose_hook, signal_trigger, handler);
    signal->_execution_context = context;
    // set default owner
    action_owner(signal) = signal;

    // store / reset schedule config
    bytecopy(with_config, &signal->_schedule_config);
    // inherit configured priority
    sorted_set_item_priority(signal) = signal->_schedule_config.priority;
}

// -------------------------------------------------------------------------------------

void signal_trigger(Action_signal_t *_this, signal_t signal) {
    Process_control_block_t *process_to_schedule = _this->_execution_context;

    // store signal to be passed to event dispatcher
    action_signal_input(_this) = signal;
    // enqueue itself to pending actions on event processor, possible priority shift
    action_queue_insert(&process_to_schedule->pending_signal_queue, _this);

    // wakeup target process if process is waiting for that
    if (process_waiting(process_to_schedule)) {
        schedule(process_to_schedule, &_this->_schedule_config);
    }
}
