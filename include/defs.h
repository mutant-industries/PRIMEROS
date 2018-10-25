/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  PRIMEROS kernel configuration file
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_DEFS_H_
#define _SYS_DEFS_H_

#include <driver/config.h>


/**
 * custom max WDT timer ticks when handling resource list [default 512]
 *  - __RESOURCE_MANAGEMENT_ENABLE__ must be defined to take effect
 */
//#define __RESOURCE_LIST_CRITICAL_SECTION_WDT_MAX_TICKS__    512

/**
 * select custom WDT source when handling resource list [default SMCLK]
 *  - __RESOURCE_MANAGEMENT_ENABLE__ must be defined to take effect
 */
//#define __RESOURCE_LIST_CRITICAL_SECTION_WDT_SSEL__         SMCLK

// -------------------------------------------------------------------------------------

/**
 * Defined in process.h, needed by both process.h and resource.h
 */
typedef struct Process_control_block Process_control_block_t;


#endif /* _SYS_DEFS_H_ */
