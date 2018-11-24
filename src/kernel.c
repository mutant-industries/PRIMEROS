// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <kernel.h>
#include <driver/interrupt.h>


int16_t kernel_start(Process_control_block_t *init_process, uint16_t init_process_priority, void (*sys_init)(void), bool wakeup,
             Context_switch_handle_t *context_switch_handle, Timer_channel_handle_us_convertible_t *timing_handle) {

    int16_t processing_init_result;
    Process_create_config_t init_process_config;

    // only wakeup if it makes sense
    wakeup &= init_process->alive;

    if (processing_init_result = processing_reinit(context_switch_handle, ! wakeup)) {
        return processing_init_result;
    }

    if ( ! wakeup) {
        zerofill(&init_process_config);
        // init process shall be created from current stack with given priority
        init_process_config.stack_addr_low = NULL;
        init_process_config.priority = init_process_priority;
        // create and schedule init process
        process_create(init_process, &init_process_config);
        // init process will become owner of all resources registered in sys_init()
        running_process = init_process;
        // execute user initialization
        (*sys_init)();
    }

    // wakeup - prepare for context switch
    running_process = init_process;

    yield();

    interrupt_enable();

    return KERNEL_API_SUCCESS;
}
