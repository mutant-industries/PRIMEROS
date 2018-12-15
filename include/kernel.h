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
#include <scheduler.h>
#include <time.h>


/**
 * PrimerOS kernel entry point
 *
 *  - the caller becomes init process and is schedulable with init process priority
 *  - when kernel is up, sys_init is executed and context switch is initiated
 *  - if wakeup is set, only modules that depend on given handler are initialized
 *
 * @param context_switch_handle - context switch initiation and timing
 * @param timing_handle - kernel timing API (sleep(), delayed execution, blocking wait with timeout...)
 */
signal_t kernel_start(Process_control_block_t *init_process, priority_t init_process_priority, void (*sys_init)(void), bool wakeup,
              Context_switch_handle_t *context_switch_handle, Timer_channel_handle_us_convertible_t *timing_handle);


#endif /* _SYS_KERNEL_H_ */
