/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  PrimerOS kernel configuration file and common definitions
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_DEFS_H_
#define _SYS_DEFS_H_

#include <stdint.h>
#include <driver/config.h>


/**
 * do not start general-purpose signal processor, do not allocate memory for signal processor
 *  - this will disable all timed signals, sleep() and blocking wait with timeout
 *  - also drivers shall no longer be able to make use of default signal processor
 *  - eventing can still be used however events must be created with custom execution context since there is no default context
 */
//#define __SIGNAL_PROCESSOR_DISABLE__

/**
 * Allocate memory for event triggered on system wakeup (when kernel_start 'wakeup' parameter is set)
 *  - makes sense only on devices with some non-volatile memory such as FRAM
 *  - subscriptions to wakeup event can be used to reinitialize drivers after system start
 *  - if used then all such subscriptions must be allocated to non-volatile memory
 */
//#define __WAKEUP_EVENT_ENABLE__

/**
 * stack size of signal default execution context - set according to what type of actions are going to be subscribed to events,
 * the default value [0xFE] is just estimated worst case for development environment
 *  - the absolute minimum depends on memory model used, plus always consider reserved space for interrupt servicing
 *  - please also consider that compiler optimization level affects minimum stack size
 */
//#define __SIGNAL_PROCESSOR_STACK_SIZE__        ((uint16_t) (0xFE))

/**
 * to convert seconds or milliseconds to microseconds, the multiplication by 1000 is required, {@see time_unit_from()},
 * and there might be no 'mul' instruction or no external HW multiplier
 *  - the default multiplication by 1000 using bit shifts is only optimized for 16-bit instructions
 *  - if external HW multiplier is used then it's usage should be thread-safe
 */
//#define __TIMING_CONVERSION_AVOID_HW_MULTIPLICATION__

/**
 * priority with which timed signals shall be triggered, default [0xFF00]
 */
//#define __TIMING_QUEUE_HANDLER_PRIORITY__     ((uint16_t) (0xFF00))

/**
 * clear interrupt flag on context switch handle inside interrupt service
 *  - must be defined if interrupt flag is not cleared automatically by hardware
 */
//#define __CONTEXT_SWITCH_HANDLE_CLEAR_IFG__

// -------------------------------------------------------------------------------------

/**
 * Action execution argument, process exit code
 */
typedef void * signal_t;

/**
 * Process priority / general sortable element priority
 */
typedef uint16_t priority_t;

/**
 * Reserved priority level - passing to schedulable_state_reset() causes process to become last schedulable process
 * among processes on the same priority level
 */
#define PRIORITY_RESET  ((priority_t) (-1))

#define signal(signal) ((signal_t) (signal))

/**
 * Kernel API general return codes
 */
#define KERNEL_API_SUCCESS                  signal(0x0000)
#define KERNEL_API_INVALID_ARGUMENT         signal(0x4000)
#define KERNEL_API_INVALID_STATE            signal(0x2000)
#define KERNEL_DISPOSED_RESOURCE_ACCESS     signal(-9)
#define KERNEL_API_TIMEOUT                  signal(-8)

// -------------------------------------------------------------------------------------

/**
 * Defined in process.h
 */
typedef struct Process_control_block Process_control_block_t;

/**
 * Defined in time.h
 */
typedef struct Time_unit Time_unit_t;

/**
 * Defined in action/queue.h
 */
typedef struct Action_queue Action_queue_t;

/**
 * Defined in scheduler.c
 */
extern Process_control_block_t *running_process;


#endif /* _SYS_DEFS_H_ */
