/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Process lifecycle, scheduler
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_PROCESS_H_
#define _SYS_PROCESS_H_

#include <stdint.h>
#include <stdbool.h>
#include <collection/list.h>
#include <sync/semaphore.h>
#include <time.h>

// -------------------------------------------------------------------------------------

/**
 * Process api return codes
 */
#define PROCESS_SUCCESS                                 KERNEL_API_SUCCESS
#define PROCESS_CONTEXT_SWITCH_HANDLE_EMPTY             (-1)
#define PROCESS_CONTEXT_SWITCH_HANDLE_UNSUPPORTED       (-2)
#define PROCESS_KILLED                                  KERNEL_DISPOSED_RESOURCE_ACCESS

// -------------------------------------------------------------------------------------

#define Context_switch_handle_t Vector_handle_t

// -------------------------------------------------------------------------------------

/**
 * Process state holder and control structure
 */
struct Process_control_block {
    // enable round-robin chaining and dispose(Process_control_block_t *), process owner is parent process
    List_item_t _chainable;
#ifdef __RESOURCE_MANAGEMENT_ENABLE__
    // list of resources owned by process
    Disposable_t *_resource_list;
#endif
    // top-of-stack of process
    uintptr_t _stack_pointer;
    // set once on process start
    uint16_t _original_priority;
    // semaphore used for blocking wait on process termination
    Semaphore_t _process_exit_sync;
    // exit code the process terminated with
    int16_t _exit_code;
    // process execution status
    bool alive;
    // last update of effective priority - starts at _original_priority and is only allowed to increase
    uint16_t last_set_priority;
    // actual priority used to sort processes
    uint16_t effective_priority;
    // the queue the process is linked to
    struct Process_control_block **queue;
    // return value passing from blocking states
    int16_t blocked_state_return_value;
    // hook called on process exit / dispose
    void (*exit_hook)(int16_t exit_code);
    // scheduler hooks
    void (*pre_schedule_hook)(void);
    void (*post_suspend_hook)(void);

};

/**
 * Process init configuration
 */
typedef struct Process_create_config {
    // lowest address of memory allocated for stack - optional, use current stack if not set
    uint8_t *stack_addr_low;
    // size of memory block allocated for stack (applies if stack_addr_low set)
    uint16_t stack_size;
    // initial process priority
    uint16_t priority;
    // address to start execution at (applies if stack_addr_low set)
    int16_t (*entry_point)(void *);
    // argument to be passed to process on start (applies if stack_addr_low set)
    void *argument;
    // hooks can be preset here by parent process or using process_current_set_XXX macros within process itself
    void (*exit_hook)(int16_t exit_code);
    void (*pre_schedule_hook)(void);
    void (*post_suspend_hook)(void);

} Process_create_config_t;

// -------------------------------------------------------------------------------------

/**
 * Set context switch handle during system start and initialize processing environment
 *  - can also be called anytime while system is running to change the handle
 */
int16_t processing_reinit(Context_switch_handle_t *handle, bool reset_state);

/**
 * Create and schedule process with given configuration
 *  - if new process is runnable and has higher priority than running process, context switch shall be initiated
 *  - current process becomes owner of the new process if resource management is enabled
 */
void process_create(Process_control_block_t *process, Process_create_config_t *config);

/**
 * Wait for given process to terminate, return actual return value of given process
 *  - return KERNEL_DISPOSED_RESOURCE_ACCESS if process terminated already or if process was killed
 */
int16_t process_wait(Process_control_block_t *process);

/**
 * Kill given process, release all allocated resources if resource management is enabled
 */
void process_kill(Process_control_block_t *process);

/**
 * Initiate context switch, it only makes sense to yield, when
 *  - preemptive scheduler is used, process wants to give up remaining CPU time on behalf of other process with the same priority
 *  - process removed itself from runnable process queue, shall wakeup on some event and no longer wishes to use CPU
 *  - calling yield in other cases is completely harmless, context switch is not initiated
 */
void yield(void);

/**
 * Return address of this function is initialized on process stack, shall be reached when process main function returns
 *  - also can be called any other time with the same effect
 *  - return value is passed to all processes waiting for current to terminate in process_wait()
 */
void process_exit(int16_t exit_code);

// -------------------------------------------------------------------------------------

/**
 * Insert process to runnable process queue
 *  - also initiate context switch if process has higher priority than running process
 */
void process_schedule(Process_control_block_t *process);

/**
 * Place process to given queue, order by effective priority
 */
bool process_enqueue(Process_control_block_t **queue, Process_control_block_t *process);

// -------------------------------------------------------------------------------------

/**
 * Exit event hook
 *  - executed on process (forced) exit, exit code of process is passed as parameter
 */
#define process_current_set_exit_hook(hook) \
    running_process->exit_hook = hook;

/**
 * Special event hooks
 *  - executed within scheduler interrupt service if set
 */
#define process_current_set_pre_schedule_hook(hook) \
    running_process->pre_schedule_hook = hook;

#define process_current_set_post_suspend_hook(hook) \
    running_process->post_suspend_hook = hook;


#endif /* _SYS_PROCESS_H_ */
