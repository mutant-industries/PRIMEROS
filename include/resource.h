/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Resource management - owned resource concept
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_RESOURCE_H_
#define _SYS_RESOURCE_H_

#include <defs.h>
#include <driver/disposable.h>

// -------------------------------------------------------------------------------------

#ifdef __RESOURCE_MANAGEMENT_ENABLE__

/**
 * Resource owned by process to be cleaned up on process exit
 */
struct Disposable {
    // cleanup hook of references between resource and process
    Dispose_hook_t _resource_dispose_hook;
    // owner process control block
    Process_control_block_t *_owner;
    // parent hook responsible of freeing up the resource
    dispose_function_t _dispose_hook;
    // enable resource bidirectional chaining
    struct Disposable *_next;
    struct Disposable *_prev;

};

// -------------------------------------------------------------------------------------

#define __dispose_hook_register(resource, dispose_hook) \
    resource_mark(running_process, (Disposable_t *)(resource), (dispose_function_t) (dispose_hook));

/**
 * Set resource owner, add resource to resource_list of it's owner, register optional dispose hook
 */
void resource_mark(Process_control_block_t *, Disposable_t *, dispose_function_t);


#endif /* __RESOURCE_MANAGEMENT_ENABLE__ */

// -------------------------------------------------------------------------------------

/**
 * Disposed resource signal API possible replacement
 */
extern signal_t unsupported_after_disposed(void);

/**
 * Byte-copy source structure to destination structure, zerofill destination if source empty
 *  - assume both are the same size or source is attribute of destination starting at offset 0
 */
#define bytecopy(source, destination) \
    if ( ! (source)) { \
        __do_zerofill((void *) (destination), sizeof(*(source))); \
    } \
    else { \
        __do_bytecopy((void *) (source), (void *) (destination), sizeof(*(source))); \
    }

/**
 * Simple (unoptimized) memcpy
 */
void __do_bytecopy(void *source, void *destination, uint16_t size);


#endif /* _SYS_RESOURCE_H_ */
