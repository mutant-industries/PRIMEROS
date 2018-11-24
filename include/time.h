/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Sleep, async (delayed) execution
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_TIME_H_
#define _SYS_TIME_H_

#include <stdint.h>
#include <driver/timer.h>

// -------------------------------------------------------------------------------------

/**
 * Timer channel handle with normalization functions
 */
typedef struct Timer_channel_handle_us_convertible {
    // timer handle, enable dispose(Timer_channel_handle_us_convertible_t *)
    Timer_channel_handle_t timer_handle;
    // normalization functions, assume 1 us == 1 tick if empty
    uint32_t (*ticks_to_us)(uint32_t ticks);
    uint32_t (*us_to_ticks)(uint32_t us);

} Timer_channel_handle_us_convertible_t;


#endif /* _SYS_TIME_H_ */
