// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <stddef.h>
#include <resource.h>
#include <driver/critical.h>

/**
 * Only compiles if Disposable_t and Process_control_block_t contain required mapping structures
 */
#ifdef __RESOURCE_MANAGEMENT_ENABLE__

// -------------------------------------------------------------------------------------

#ifndef __RESOURCE_LIST_CRITICAL_SECTION_WDT_MAX_TICKS__
#define __RESOURCE_LIST_CRITICAL_SECTION_WDT_MAX_TICKS__    512
#endif

#ifndef __RESOURCE_LIST_CRITICAL_SECTION_WDT_SSEL__
#define __RESOURCE_LIST_CRITICAL_SECTION_WDT_SSEL__         SMCLK
#endif

// -------------------------------------------------------------------------------------

/**
 * This point reached on dispose() if process_mark_resource was called before
 */
static dispose_function_t _resource_list_remove(Disposable_t *resource) {
    Disposable_t *prev, *current;
    dispose_function_t result = NULL;

    __critical_section_WDT_ssel_interval_enter(__RESOURCE_LIST_CRITICAL_SECTION_WDT_SSEL__,
                        __RESOURCE_LIST_CRITICAL_SECTION_WDT_MAX_TICKS__);

    if (resource && resource->_owner) {
        if (resource->_owner->_resource_list) {
            // first item on list check
            if (resource->_owner->_resource_list == resource) {
                resource->_owner->_resource_list = resource->_owner->_resource_list->_next;
            }
            else {
                // search resource in list...
                for (prev = resource->_owner->_resource_list, current = prev->_next; current && current != resource;
                     prev = current, current = current->_next);

                // ... and remove if found
                if (current) {
                    prev->_next = current->_next;
                }
            }
        }

        resource->_owner = NULL;
        // return parent dispose hook
        result = resource->_dispose_hook;
    }

    __critical_section_WDT_exit();

    return result;
}

// -------------------------------------------------------------------------------------

void resource_mark(Process_control_block_t *pcb, Disposable_t *resource, dispose_function_t dispose_hook) {

    if ( ! pcb/* || ! resource*/) {
        return;
    }

    // only pointer to pcb of running process is considered valid, since the pointer can contain random data
    if (resource->_owner == pcb) {
        _resource_list_remove(resource);
    }

    resource->_owner = pcb;
    resource->_resource_dispose_hook._dispose_hook = (dispose_function_t) _resource_list_remove;
    resource->_dispose_hook = dispose_hook;

    __critical_section_WDT_ssel_interval_enter(__RESOURCE_LIST_CRITICAL_SECTION_WDT_SSEL__,
                        __RESOURCE_LIST_CRITICAL_SECTION_WDT_MAX_TICKS__);

    resource->_next = pcb->_resource_list;
    pcb->_resource_list = resource;

    __critical_section_WDT_exit();
}

#endif /* __RESOURCE_MANAGEMENT_ENABLE__ */
