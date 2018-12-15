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
 * do not start event processor, do not allocate memory for event processor
 *  - eventing can still be used however events must be created with custom execution context, because there is no default context
 */
//#define __EVENT_PROCESSOR_DISABLE__

/**
 * stack size of event default execution context - set according to what type of actions are going to be subscribed to events,
 * the default value [0xFF] is just estimated worst case for development environment
 *  - the absolute minimum depends on memory model used, plus always consider reserved space for interrupt servicing
 *  - please also consider that compiler optimization level affects minimum stack size
 */
//#define __EVENT_PROCESSOR_STACK_SIZE__        0xFF

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
#define KERNEL_DISPOSED_RESOURCE_ACCESS     signal(-9)

// -------------------------------------------------------------------------------------

/**
 * Defined in process.h
 */
typedef struct Process_control_block Process_control_block_t;

/**
 * Defined in scheduler.c
 */
extern Process_control_block_t *running_process;


#endif /* _SYS_DEFS_H_ */
