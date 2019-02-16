// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action/signal.h>
#include <action/queue.h>
#include <driver/interrupt.h>
#include <process.h>


static void _on_signal_released(Action_signal_t *_this, Action_queue_t *origin) {
    // interrupts are disabled already

    if (origin != &action_signal_execution_context(_this)->pending_signal_queue) {
        return;
    }

    // signal handler execution started or signal was released by user
    action_signal_unhandled_trigger_count(_this)--;
}

static bool _on_signal_handled(Action_signal_t *_this) {
    // interrupts are disabled already

    if (action_signal_unhandled_trigger_count(_this) == 1) {
        return false;
    }

    action_signal_unhandled_trigger_count(_this)--;

    // signal is still in pending queue, stay if triggered more times than handled
    return true;
}

// -------------------------------------------------------------------------------------

// Action_signal_t constructor
void action_signal_register(Action_signal_t *signal, dispose_function_t dispose_hook, signal_handler_t handler,
        Schedule_config_t *with_config, Process_control_block_t *context) {

    action_create(signal, dispose_hook, signal_trigger, handler);
    // set context within which shall signal be processed
    action_signal_execution_context(signal) = context;
    // set default owner
    action_owner(signal) = signal;
    // reset trigger count
    action_signal_unhandled_trigger_count(signal) = 0;
    // reset signal to be passed to handler
    action_signal_input(signal) = NULL;
    // set default on_handled hook
    action_signal_on_handled(signal) = _on_signal_handled;
    // set default on_released hook to ensure signal._unhandled_trigger_count is consistent
    action_on_released(signal) = (action_released_hook_t) _on_signal_released;

    // store / reset schedule config
    schedule_config_copy(with_config, action_signal_schedule_config(signal));
    // inherit priority of owner process by default
    sorted_set_item_priority(signal) = process_get_original_priority(running_process);

    // evaluate configured priority if higher than current
    if (action_signal_schedule_config(signal)->priority > sorted_set_item_priority(signal)) {
        sorted_set_item_priority(signal) = action_signal_schedule_config(signal)->priority;
    }
}

// -------------------------------------------------------------------------------------

void signal_trigger(Action_signal_t *_this, signal_t signal) {
    Process_control_block_t *process_to_schedule = action_signal_execution_context(_this);

    interrupt_suspend();

    // store signal to be passed to event dispatcher
    action_signal_input(_this) = signal;

    // keep track of trigger count in case triggered faster than handled
    action_signal_unhandled_trigger_count(_this)++;

    if (action_queue(deque_item_container(_this)) != &action_signal_execution_context(_this)->pending_signal_queue) {
        // enqueue itself to pending actions on execution context, possible priority shift
        action_queue_insert(&process_to_schedule->pending_signal_queue, _this);
    }

    // wakeup target process if process is waiting for that and not scheduled already
    if (process_waiting(process_to_schedule) && process_suspended(process_to_schedule)) {
        // reset blocked state signal so distinguish signal trigger from timeout
        process_to_schedule->blocked_state_signal = SCHEDULER_SUCCESS;
        // and trigger scheduler responsible for placing process to correct queue
        schedule(process_to_schedule);
    }

    interrupt_restore();
}

bool signal_set_priority(Action_signal_t *signal, priority_t priority_lowest) {
    bool highest_priority_placement;

    interrupt_suspend();

    // inherit priority from schedule config
    if (priority_lowest < action_signal_schedule_config(signal)->priority) {
        priority_lowest = action_signal_schedule_config(signal)->priority;
    }

    highest_priority_placement = action_set_priority(signal, priority_lowest);

    interrupt_restore();

    return highest_priority_placement;
}

// -------------------------------------------------------------------------------------

#ifndef __SIGNAL_PROCESSOR_DISABLE__

#ifndef __SIGNAL_PROCESSOR_STACK_SIZE__
#define __SIGNAL_PROCESSOR_STACK_SIZE__        ((uint16_t) (0xFF))
#endif

Process_control_block_t signal_processor;

static uint8_t _signal_processor_stack[__SIGNAL_PROCESSOR_STACK_SIZE__];

void signal_processor_init() {

    // signal processor inherits priority of pending signals
    signal_processor.create_config.priority = 0;
    signal_processor.create_config.stack_addr_low = (data_pointer_register_t) _signal_processor_stack;
    signal_processor.create_config.stack_size = __SIGNAL_PROCESSOR_STACK_SIZE__;
    // null args passed to wait()
    signal_processor.create_config.arg_1 = NULL;
    signal_processor.create_config.arg_2 = NULL;
    signal_processor.create_config.entry_point = (process_entry_point_t) wait;

    // create event processor
    process_create(&signal_processor);
    // and just set it to 'waiting for event' state, no need to schedule it
    process_waiting(&signal_processor) = true;
}

#endif /* __SIGNAL_PROCESSOR_DISABLE__ */
