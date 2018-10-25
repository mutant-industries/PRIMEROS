/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Process lifecycle
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_PROCESS_H_
#define _SYS_PROCESS_H_

#include <stdint.h>
#include <stdbool.h>
#include <resource.h>

// -------------------------------------------------------------------------------------

/**
 * Process state holder and control structure.
 */
struct Process_control_block {
    // enable dispose(Process_control_block_t *), PCB owner is parent process
    Disposable_t _disposable;

#ifdef __RESOURCE_MANAGEMENT_ENABLE__
    Disposable_t *_resource_list;
#endif

};

// -------------------------------------------------------------------------------------

/**
 * PCB of running process, also set dynamically from resource owner before callback on resource is executed.
 *  - does not automatically apply for interrupt vectors
 */
extern Process_control_block_t *running_process;


#endif /* _SYS_PROCESS_H_ */
