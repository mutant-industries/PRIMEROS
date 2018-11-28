// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <stddef.h>
#include <resource.h>
#include <process.h>
#include <driver/interrupt.h>


/**
 * Only compiles if Disposable_t and Process_control_block_t contain required mapping structures
 */
#ifdef __RESOURCE_MANAGEMENT_ENABLE__

/**
 * This point reached on dispose() if process_mark_resource was called before
 */
static dispose_function_t _resource_list_remove(Disposable_t *resource) {
    dispose_function_t result = NULL;

    interrupt_suspend();

    if (resource && resource->_owner) {
        // first item on list check
        if (resource->_owner->_resource_list == resource) {
            resource->_owner->_resource_list = resource->_owner->_resource_list->_next;
        }

        // remove from list
        if (resource->_prev) {
            resource->_prev->_next = resource->_next;
        }

        if (resource->_next) {
            resource->_next->_prev = resource->_prev;
        }

        // optimization - when the same resource is marked again, _resource_list_remove shall not be called
        resource->_owner = NULL;

        // return parent dispose hook
        result = resource->_dispose_hook;
    }

    interrupt_restore();

    return result;
}

// -------------------------------------------------------------------------------------

void resource_mark(Process_control_block_t *owner, Disposable_t *resource, dispose_function_t dispose_hook) {

    if ( ! owner) {
        // resource shall not be released on process exit, but the dispose feature shall be preserved
        resource->_resource_dispose_hook._dispose_hook = dispose_hook;
        resource->_owner = NULL;

        return;
    }

    // only pointer to owner of running process is considered valid, since the pointer can contain random data
    if (resource->_owner == owner) {
        _resource_list_remove(resource);
    }

    resource->_owner = owner;
    resource->_resource_dispose_hook._dispose_hook = (dispose_function_t) _resource_list_remove;
    resource->_dispose_hook = dispose_hook;
    resource->_prev = NULL;

    interrupt_suspend();

    resource->_next = owner->_resource_list;

    if (owner->_resource_list) {
        owner->_resource_list->_prev = resource;
    }

    owner->_resource_list = resource;

    interrupt_restore();
}

#endif /* __RESOURCE_MANAGEMENT_ENABLE__ */

// -------------------------------------------------------------------------------------

int16_t unsupported_after_disposed() {
    return KERNEL_DISPOSED_RESOURCE_ACCESS;
}

void __do_bytecopy(void *source, void *destination, uint16_t size) {
    uintptr_t i, j;

    for (i = (uintptr_t) source, j = (uintptr_t) destination; i < (((uintptr_t) source) + size); i++, j++) {
        *((uint8_t *) j) = *((uint8_t *) i);
    }
}
