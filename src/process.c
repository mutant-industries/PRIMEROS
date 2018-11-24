// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <process.h>
#include <driver/interrupt.h>
#include <driver/stack.h>
#include <compiler.h>


Process_control_block_t *running_process;

static Context_switch_handle_t *_context_switch_handle;
// noinit to allow wakeup from deep sleep modes, where wakeup goes through C runtime startup code
__noinit static Process_control_block_t *_process_queue;

void process_context_switch(void);

// -------------------------------------------------------------------------------------

static dispose_function_t _process_release(Process_control_block_t *_this) {

    // prevent the process from being scheduled
    _this->alive = false;

    // execute exit hook if set
    if (_this->exit_hook) {
        // if process was killed by another process, then pass PROCESS_KILLED
        (*_this->exit_hook)(_this->_exit_code);
    }

#ifdef __RESOURCE_MANAGEMENT_ENABLE__
    // release all resources that belong to current process except process_exit_sync, which is always the last element of list
    while (_this->_resource_list && _this->_resource_list != (Disposable_t *) &_this->_process_exit_sync) {
        Disposable_t *resource = _this->_resource_list;

        // child process disposal
        if (resource->_dispose_hook == _process_release) {
            process_kill((Process_control_block_t *) resource);
        }

        dispose(resource);
    }
#endif

    interrupt_suspend();

    // remove process from whatever queue it is linked to
    list_remove((List_item_t **) _this->queue, (List_item_t *) _this);

    // wakeup all waiting processes with signal from exit code
    _this->_process_exit_sync.signal_all(&_this->_process_exit_sync, _this->_exit_code);

    // make sure no other process gets blocked waiting for this one to terminate
    dispose(&_this->_process_exit_sync);

    // only yield on process exit, also no yield on init process dispose (kernel halt)
    if (_this == running_process && _process_queue) {
        yield();    // ... and never return
    }

    interrupt_restore();

    return NULL;
}

void process_create(Process_control_block_t *process, Process_create_config_t *config) {
    Process_control_block_t *running_process_bak;

    // initialize stack if requested, use current stack otherwise
    if (config->stack_addr_low) {
        deferred_stack_pointer_init(&process->_stack_pointer, config->stack_addr_low, config->stack_size);
        deferred_stack_push_return_address(&process->_stack_pointer, process_exit);
        deferred_stack_context_init(&process->_stack_pointer, config->entry_point, config->argument);
    }

    // reset process priority
    process->_original_priority = config->priority;
    process->last_set_priority = config->priority;
    process->effective_priority = config->priority;

    // set exit hook and scheduler hooks
    process->exit_hook = config->exit_hook;
    process->pre_schedule_hook = config->pre_schedule_hook;
    process->post_suspend_hook = config->post_suspend_hook;

#ifdef __RESOURCE_MANAGEMENT_ENABLE__
    // reset resource list
    process->_resource_list = NULL;
#endif

    interrupt_suspend();

    running_process_bak = running_process;
    // set global running process so that it will become owner of following resources
    running_process = process;
    // register semaphore used for blocking wait for this process termination
    semaphore_register(&process->_process_exit_sync, 0);
    // restore running process after resource allocation finished
    running_process = running_process_bak;

    interrupt_restore();

    // exit code is only set on proper process termination
    process->_exit_code = PROCESS_KILLED;

    // make process ready to be scheduled
    process->alive = true;

    __dispose_hook_register(process, _process_release);

    process_schedule(process);
}

void process_exit(int16_t exit_code) {
    // store exit code to be passed to exit hook and waiting processes
    running_process->_exit_code = exit_code;
    // end process, release all related resources
    dispose(running_process);
}

// -------------------------------------------------------------------------------------

int16_t process_wait(Process_control_block_t *process) {
    return process->_process_exit_sync.acquire(&process->_process_exit_sync);
}

void process_kill(Process_control_block_t *process) {
    // end process, release all related resources
    dispose(process);
    // child process might be already executing dispose on itself with lower priority - let's wait to avoid deadlock
    process_wait(process);
}

// -------------------------------------------------------------------------------------

bool process_enqueue(Process_control_block_t **queue, Process_control_block_t *process) {
    List_item_t *current, *process_list_item;
    bool highest_priority_placement = false;

    interrupt_suspend();

    // queue empty or highest priority process
    if ( ! *queue || (*queue)->effective_priority < process->effective_priority) {
        list_add_last((List_item_t **) queue, (List_item_t *) process);
        // new head of the queue
        *queue = process;
        // enqueued process has highest priority
        highest_priority_placement = true;
    }
    else {
        current = (List_item_t *) *queue;

        // place behind the first element with higher or equal priority or place to end of queue
        while (((Process_control_block_t *) current->_next)->effective_priority >= process->effective_priority
               && current->_next != (List_item_t *) *queue) {
            current = current->_next;
        }

        process_list_item = (List_item_t *) process;

        process_list_item->_next = current->_next;
        process_list_item->_prev = current;
        current->_next->_prev = process_list_item;
        current->_next = process_list_item;
    }

    // link given queue to process control block
    process->queue = queue;

    interrupt_restore();

    return highest_priority_placement;
}

// -------------------------------------------------------------------------------------

int16_t processing_reinit(Context_switch_handle_t *handle, bool reset_state) {
    Vector_handle_t *vector = (Vector_handle_t *) handle;

    // sanity check
    if ( ! handle) {
        return PROCESS_CONTEXT_SWITCH_HANDLE_EMPTY;
    }

    // try register handle interrupt service handler
    if (vector->register_raw_handler(vector, process_context_switch, true)
            || vector->clear_interrupt_flag(vector) || vector->set_enabled(vector, true)) {

        return PROCESS_CONTEXT_SWITCH_HANDLE_UNSUPPORTED;
    }

    // reset (possibly persistent) process queue
    if (reset_state) {
        _process_queue = NULL;
    }

    // suspend current handle if set
    if (_context_switch_handle) {
        ((Vector_handle_t *) _context_switch_handle)->set_enabled(((Vector_handle_t *) _context_switch_handle), false);
    }

    _context_switch_handle = handle;

    return PROCESS_SUCCESS;
}

// -------------------------------------------------------------------------------------

void process_schedule(Process_control_block_t *process) {

    if (process->alive && process_enqueue(&_process_queue, process)) {
        yield();
    }
}

void yield() {

    // only initiate context switch if it makes sense
    if (running_process != _process_queue) {
        ((Vector_handle_t *) _context_switch_handle)->trigger((Vector_handle_t *) _context_switch_handle);
    }
}

__naked __interrupt void process_context_switch() {

    stack_save_context(&running_process->_stack_pointer);

    if (running_process->post_suspend_hook) {
        (*running_process->post_suspend_hook)();
    }

    running_process = _process_queue;

    if (running_process->pre_schedule_hook) {
        (*running_process->pre_schedule_hook)();
    }

    stack_restore_context(&running_process->_stack_pointer);

    reti;
}
