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
 * disable subscription of actions to events to ensure strictly deterministic blocking api
 */
//#define __ASYNC_API_DISABLE__

// -------------------------------------------------------------------------------------

/**
 * Kernel api general return codes
 */
#define KERNEL_API_SUCCESS                  (0x0000)
#define KERNEL_API_INVALID_ARGUMENT         (0x4000)
#define KERNEL_DISPOSED_RESOURCE_ACCESS     (0x8000)

// -------------------------------------------------------------------------------------
/**
 * Defined in process.h, needed by process.h, resource.h and event.h
 */
typedef struct Process_control_block Process_control_block_t;


/**
 * Process schedule configuration, needed by process.c and all kernel blocking api functions
 */
typedef struct Process_schedule_config {
    // priority to be set when scheduled
    uint16_t priority;

} Process_schedule_config_t;

/**
 * Defined in process.c, needed by both process.h and resource.h
 */
extern Process_control_block_t *running_process;


#endif /* _SYS_DEFS_H_ */
