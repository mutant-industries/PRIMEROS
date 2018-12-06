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
 * disable subscription of actions to events to ensure strictly deterministic blocking API
 */
//#define __ASYNC_API_DISABLE__

// -------------------------------------------------------------------------------------

/**
 * Action execution argument, process exit code
 */
typedef void * signal_t;

/**
 * Process priority / general sortable element priority
 */
typedef uint16_t priority_t;

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
