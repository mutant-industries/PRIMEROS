/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  PrimerOS kernel entry point
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_KERNEL_H_
#define _SYS_KERNEL_H_

#include <stdbool.h>
#include <defs.h>
#include <event.h>
#include <scheduler.h>
#include <time.h>


/**
 * System wakeup event triggered when kernel started with 'wakeup' set
 *  - any persistent process can subscribe (e.g. to reinitialize drivers) after system restart
 *  - use only when process control structures are stored in some non-volatile memory such as FRAM
 */
extern Event_t wakeup_event;

/**
 * Signal the wakeup event is triggered with on system wakeup
 */
#define SYSTEM_WAKEUP_SIGNAL    KERNEL_API_SUCCESS

/**
 * PrimerOS kernel entry point
 *
 *  - the caller becomes init process and is schedulable with init process priority
 *  - when kernel is up, sys_init is executed and context switch is initiated
 *  - if wakeup is set, only modules that depend on given handlers are initialized
 *
 * @param context_switch_handle - context switch trigger
 * @param timing_handle - kernel timing API (sleep(), delayed execution, blocking wait with timeout...)
 */
signal_t kernel_start(Process_control_block_t *init_process, priority_t init_process_priority, void (*sys_init)(void),
        bool wakeup, Context_switch_handle_t *context_switch_handle, Timing_handle_t *timing_handle);


#endif /* _SYS_KERNEL_H_ */
