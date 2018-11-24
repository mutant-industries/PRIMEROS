/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  PrimerOS kernel configuration file and common definitions
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_DEFS_H_
#define _SYS_DEFS_H_

#include <driver/config.h>



// -------------------------------------------------------------------------------------

/**
 * Kernel api general return codes
 */
#define KERNEL_API_SUCCESS                  (0x0000)
#define KERNEL_DISPOSED_RESOURCE_ACCESS     (0x8000)

// -------------------------------------------------------------------------------------
/**
 * Defined in process.h, needed by process.h, resource.h and event.h
 */
typedef struct Process_control_block Process_control_block_t;

/**
 * Defined in process.c, needed by both process.h and resource.h
 */
extern Process_control_block_t *running_process;


#endif /* _SYS_DEFS_H_ */
