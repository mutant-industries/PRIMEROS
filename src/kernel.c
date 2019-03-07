// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <kernel.h>
#include <driver/interrupt.h>
#include <action/signal.h>
#include <compiler.h>
#include <event.h>
#include <process.h>


#if defined(__WAKEUP_EVENT_ENABLE__) && ! defined(__SIGNAL_PROCESSOR_DISABLE__)
__persistent Event_t wakeup_event = {0};
#endif

// -------------------------------------------------------------------------------------

signal_t kernel_start(Process_control_block_t *init_process, priority_t init_process_priority, void (*sys_init)(void), bool wakeup,
             Context_switch_handle_t *context_switch_handle, Timing_handle_t *timing_handle) {

    signal_t module_init_result;

    // only wakeup if it makes sense
    wakeup &= process_is_schedulable(init_process);

    // initialize context switching and runnable queue
    if (module_init_result = scheduler_reinit(context_switch_handle, ! wakeup)) {
        return module_init_result;
    }

    if ( ! wakeup) {
        zerofill(&init_process->create_config);
        // init process shall be created from current stack with given priority
        init_process->create_config.priority = init_process_priority;
        // create init process
        process_create(init_process);
    }

    // place init process to runnable queue
    process_schedule(init_process, 0);

    // init process shall become owner of all resources registered from now on
    running_process = init_process;

#ifndef __SIGNAL_PROCESSOR_DISABLE__
    if ( ! wakeup) {
        // create signal processor
        signal_processor_init();
    }

    if (timing_handle && (module_init_result = timing_reinit(timing_handle, &signal_processor, ! wakeup))) {
        return module_init_result;
    }
#endif

    if ( ! wakeup) {
#if  ! defined(__SIGNAL_PROCESSOR_DISABLE__) && defined(__WAKEUP_EVENT_ENABLE__)
        // create wakeup event with context of default signal processor
        event_create(&wakeup_event);
#endif
        // execute user initialization
        (*sys_init)();
    }
#if  ! defined(__SIGNAL_PROCESSOR_DISABLE__) && defined(__WAKEUP_EVENT_ENABLE__)
    else {
        // trigger wakeup event
        event_trigger(&wakeup_event, SYSTEM_WAKEUP_SIGNAL);
    }
#endif

    yield();

    interrupt_enable();

    return KERNEL_API_SUCCESS;
}
