// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <kernel.h>
#include <driver/interrupt.h>
#include <action/signal.h>
#include <event.h>
#include <process.h>


// -------------------------------------------------------------------------------------

signal_t kernel_start(Process_control_block_t *init_process, priority_t init_process_priority, void (*sys_init)(void), bool wakeup,
             Context_switch_handle_t *context_switch_handle, Timing_handle_t *timing_handle) {

    signal_t module_init_result;

    // only wakeup if it makes sense
    wakeup &= process_is_alive(init_process);

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
        // ... and schedule it
        process_schedule(init_process, 0);
    }

    // init process will become owner of all resources registered
    running_process = init_process;

#ifndef __SIGNAL_PROCESSOR_DISABLE__
    signal_processor_init();

    if (timing_handle && (module_init_result = timing_reinit(timing_handle, &signal_processor, ! wakeup))) {
        return module_init_result;
    }
#endif

    // execute user initialization
    if ( ! wakeup) {
        (*sys_init)();
    }

    yield();

    interrupt_enable();

    return KERNEL_API_SUCCESS;
}
