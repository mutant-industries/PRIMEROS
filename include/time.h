/* SPDX-License-Identifier: BSD-3-Clause */
/*
 *  Timed signal, time unit conversion, async (delayed) execution
 *
 *  Copyright (c) 2018-2019 Mutant Industries ltd.
 */

#ifndef _SYS_TIME_H_
#define _SYS_TIME_H_

#include <stdbool.h>
#include <stdint.h>
#include <driver/timer.h>
#include <defs.h>
#include <action/signal.h>
#include <scheduler.h>

// -------------------------------------------------------------------------------------

#define timed_signal(_signal) ((Timed_signal_t *) (_signal))

/**
 * Timing API return codes
 */
#define TIMING_SUCCESS                                  KERNEL_API_SUCCESS
#define TIMING_INVALID_STATE                            KERNEL_API_INVALID_STATE
#define TIMING_HANDLE_EMPTY                             signal(-1)
#define TIMING_HANDLE_UNSUPPORTED                       signal(-2)
#define TIMING_SIGNAL_TIMEOUT                           KERNEL_API_TIMEOUT

/**
 * Longest supported delay of timed signal (unchecked), approx 3 years
 */
#define MAX_DELAY_HRS                                   ((uint16_t) 0x7F00)

/**
 * Timed signal public API access
 */
#define timed_signal_create(...) _TIMED_SIGNAL_CREATE_GET_MACRO(__VA_ARGS__, _timed_signal_create_5, _timed_signal_create_4, _timed_signal_create_3, _timed_signal_create_2)(__VA_ARGS__)
#define timed_signal_set_periodic(_signal, periodic) (timed_signal(_signal)->set_periodic(timed_signal(_signal), periodic))
#define timed_signal_schedule(_signal) timed_signal(_signal)->schedule(timed_signal(_signal))

//<editor-fold desc="variable-args - timed_signal_create()">
#define _TIMED_SIGNAL_CREATE_GET_MACRO(_1,_2,_3,_4,_5,NAME,...) NAME
#ifndef __SIGNAL_PROCESSOR_DISABLE__
#define _timed_signal_create_2(_signal, _handler) \
    timed_signal_register(timed_signal(_signal), ((signal_handler_t) (_handler)), false, NULL, &signal_processor)
#define _timed_signal_create_3(_signal, _handler, _periodic) \
    timed_signal_register(timed_signal(_signal), ((signal_handler_t) (_handler)), _periodic, NULL, &signal_processor)
#define _timed_signal_create_4(_signal, _handler, _periodic, _with_config) \
    timed_signal_register(timed_signal(_signal), ((signal_handler_t) (_handler)), _periodic, _with_config, &signal_processor)
#endif
#define _timed_signal_create_5(_signal, _handler, _periodic, _with_config, _context) \
    timed_signal_register(timed_signal(_signal), ((signal_handler_t) (_handler)), _periodic, _with_config, _context)
//</editor-fold>

// getter, setter
#define timed_signal_trigger_time(_signal) (&(timed_signal(_signal)->_trigger_time))
#define timed_signal_delay(_signal) (&(timed_signal(_signal)->delay))
#define timed_signal_is_periodic(_signal) (timed_signal(_signal)->_periodic)

#define timed_signal_set_delay_from(_signal, hrs, secs, millisecs, usecs) time_unit_from(timed_signal_delay(_signal), hrs, secs, millisecs, usecs)
// (signal, usecs) -> signal.delay, (signal, secs, usecs) -> signal.delay
#define timed_signal_set_delay_usecs(...) _TIMED_SIGNAL_SET_DELAY_USECS_GET_MACRO(__VA_ARGS__, _timed_signal_set_delay_usecs_3, _timed_signal_set_delay_usecs_2)(__VA_ARGS__)
// (signal, millisecs) -> signal.delay, (signal, secs, millisecs) -> signal.delay
#define timed_signal_set_delay_millisecs(...) _TIMED_SIGNAL_SET_DELAY_MILLISECS_GET_MACRO(__VA_ARGS__, _timed_signal_set_delay_millisecs_3, _timed_signal_set_delay_millisecs_2)(__VA_ARGS__)
// (signal, secs) -> signal.delay, (signal, secs, millisecs, usecs) -> signal.delay
#define timed_signal_set_delay_secs(...) _TIMED_SIGNAL_SET_DELAY_SECS_GET_MACRO(__VA_ARGS__, _timed_signal_set_delay_secs_4, _timed_signal_set_delay_secs_3, _timed_signal_set_delay_secs_2)(__VA_ARGS__)
// (signal, hrs) -> signal.delay, (signal, hrs, secs) -> signal.delay, (signal, hrs, secs, millisecs) -> signal.delay, (signal, hrs, secs, millisecs, usecs) -> signal.delay
#define timed_signal_set_delay_hrs(...) _TIMED_SIGNAL_SET_DELAY_HRS_GET_MACRO(__VA_ARGS__, _timed_signal_set_delay_hrs_5, _timed_signal_set_delay_hrs_4, _timed_signal_set_delay_hrs_3, _timed_signal_set_delay_hrs_2)(__VA_ARGS__)

//<editor-fold desc="variable-args - timed_signal_set_delay_usecs()">
#define _TIMED_SIGNAL_SET_DELAY_USECS_GET_MACRO(_1,_2,_3,NAME,...) NAME
#define _timed_signal_set_delay_usecs_2(_signal, usecs) timed_signal_set_delay_from(_signal, 0, 0, 0, usecs)
#define _timed_signal_set_delay_usecs_3(_signal, secs, usecs) timed_signal_set_delay_from(_signal, 0, secs, 0, usecs)
//</editor-fold>
//<editor-fold desc="variable-args - timed_signal_set_delay_millisecs()">
#define _TIMED_SIGNAL_SET_DELAY_MILLISECS_GET_MACRO(_1,_2,_3,NAME,...) NAME
#define _timed_signal_set_delay_millisecs_2(_signal, millisecs) timed_signal_set_delay_from(_signal, 0, 0, millisecs, 0)
#define _timed_signal_set_delay_millisecs_3(_signal, secs, millisecs) timed_signal_set_delay_from(_signal, 0, secs, millisecs, 0)
//</editor-fold>
//<editor-fold desc="variable-args - timed_signal_set_delay_secs()">
#define _TIMED_SIGNAL_SET_DELAY_SECS_GET_MACRO(_1,_2,_3,_4,NAME,...) NAME
#define _timed_signal_set_delay_secs_2(_signal, secs) timed_signal_set_delay_from(_signal, 0, secs, 0, 0)
#define _timed_signal_set_delay_secs_4(_signal, secs, millisecs, usecs) timed_signal_set_delay_from(_signal, 0, secs, millisecs, usecs)
//</editor-fold>
//<editor-fold desc="variable-args - timed_signal_set_delay_hrs()">
#define _TIMED_SIGNAL_SET_DELAY_HRS_GET_MACRO(_1,_2,_3,_4,_5,NAME,...) NAME
#define _timed_signal_set_delay_hrs_2(_signal, hrs) timed_signal_set_delay_from(_signal, hrs, 0, 0, 0)
#define _timed_signal_set_delay_hrs_3(_signal, hrs, secs) timed_signal_set_delay_from(_signal, hrs, secs, 0, 0)
#define _timed_signal_set_delay_hrs_4(_signal, hrs, secs, millisecs) timed_signal_set_delay_from(_signal, hrs, secs, millisecs, 0)
#define _timed_signal_set_delay_hrs_5(_signal, hrs, secs, millisecs, usecs) timed_signal_set_delay_from(_signal, hrs, secs, millisecs, usecs)
//</editor-fold>

// duplicate source time unit to target
#define time_unit_copy(source, target) (target)->hrs = (source)->hrs; (target)->usecs = (source)->usecs;
#define time_unit_reset(time_unit) (time_unit)->hrs = 0; (time_unit)->usecs = 0;

// -------------------------------------------------------------------------------------

typedef struct Timed_signal Timed_signal_t;

/**
 * Time unit with granularity of 1 usecond, range 0 - 2^16 hours (max value ~ 7 years), usage:
 *  - absolute time holder - either current time or timed signal trigger time ({@see get_current_time()})
 *  - time difference holder - ({@see time_unit_from()})
 */
struct Time_unit {
    // number of microseconds, never exceeds 3600 * 1000 * 1000
    uint32_t usecs;
    // number of hours
    uint16_t hrs;

};

/**
 * Timed signal - delayed / periodic signal trigger
 */
struct Timed_signal {
    // signal, given handler executed within given context when current time >= trigger_time
    Action_signal_t _signalable;

    // -------- state --------
    // absolute time when action shall be triggered
    Time_unit_t _trigger_time;
    // delay since timed signal scheduled / delay between triggers if periodic
    Time_unit_t delay;
    // reschedule with given delay on trigger if periodic
    bool _periodic;

    // -------- public --------
    // set whether signal suppose to be triggered periodically
    signal_t (*set_periodic)(Timed_signal_t *_this, bool periodic);
    // schedule / reschedule signal to be triggered after preset delay
    signal_t (*schedule)(Timed_signal_t *_this);

};

/**
 * Initialize timed signal
*  - timed signal is standard signal (Action_signal_t) that can be scheduled to be triggered after specified delay
*  - the delay must be set at least once, helper macros timed_signal_set_delay_*() can be used for this purpose
*  - periodic signal has constant (preset) delay between triggers no matter what current overhead of timing or scheduler is
*    - it is automatically rescheduled after handled, next trigger time = last trigger time + delay
*  - the public API of timed signal (release, schedule, set periodic) can be used anytime, even from handler itself
*    - please note, that if signal is released / rescheduled within signal handler then it is removed from pending_signals
*  on process within which handler is executed ('context') which might result in priority decrease of actual 'context'
*  - signal handler receives two parameters - signal owner (signal itself by default) and TIMING_SIGNAL_TIMEOUT
*  - timed signal can be seen as standard action and used that way, e.g. it can be triggered from multiple sources
 * where each source is identified by specific signal (parameter), nut just by timing subsystem (TIMING_SIGNAL_TIMEOUT),
 * this way the same handler can cover both situations - event occurred and event timed out
*  - timed signal can also be placed to any action queue, but during call to timed_signal_schedule() it shall be removed,
 * it does not make much sense anyway, for periodic signals it completely does not make sense
 */
void timed_signal_register(Timed_signal_t *signal, signal_handler_t handler, bool periodic,
        Schedule_config_t *with_config, Process_control_block_t *context);

// -------------------------------------------------------------------------------------

/**
 * Fill given 'time_unit' from given parameters and return it
 *  - the amount of microseconds given by ((1000 * s + ms) * 1000 + usecs) must not exceed 2^32, unchecked
 */
Time_unit_t *time_unit_from(Time_unit_t *time_unit, uint16_t hrs, uint16_t secs, uint16_t millisecs, uint32_t usecs);

/**
 * Fill given 'target' structure by current absolute time
*  - if no timed signals are scheduled and time tracking is not enabled, then just return false
 */
bool get_current_time(Time_unit_t *target);

/**
 * Enable / disable current time tracking so that get_current_time() always returns true, but timing handle is kept active
 * even when no timed signals are scheduled, which usually results in increased power consumption
*  - please note, that this function might be called by multiple sources and there is no 'global' state - instead
 * each call to enable time tracking increments internal counter and call to disable tracking decrements it, so that
 * time tracking is actually disabled only when that counter reaches zero
*  - this function should be used with care if called from process - process might request time tracking and
 * then get killed, as a result timing handle shall never be deactivated since then
 */
void set_track_current_time(bool track);

/**
 * Fill given 'target' structure by absolute time of next upcoming event
*  - if no timed signals are scheduled then just return false
 */
bool get_upcoming_event_time(Time_unit_t *target);

// -------------------------------------------------------------------------------------

/**
 * Timer channel handle with conversion functions
 */
typedef struct Timing_handle {
    // resource, timer handle
    Timer_channel_handle_t timer_handle;

    // -------- state --------
    // dynamically generated constants
    uint32_t _timer_counter_mask;
    uint32_t _timer_overflow_ticks_increment;
    uint32_t _timer_overflow_us_increment;
    uint32_t _timer_compare_value_set_threshold;
    // timer counter content that corresponds to _current_time_last_stable - fix to (possible) inaccuracy in ticks_to_usecs()
    uint32_t _timer_counter_last_stable;

    // -------- public --------
    // conversion functions, assume 1 us == 1 tick if not set
    uint32_t (*ticks_to_usecs)(uint32_t ticks);
    uint32_t (*usecs_to_ticks)(uint32_t us);
    // counter bit width, accepted range 8 - 32
    uint8_t timer_counter_bit_width;

} Timing_handle_t;

/**
 * Reinitialize timing subsystem
 *  - timing_queue_processor - process the context of which shall be used as default context to handle timed signal queue
 *  - can also be called anytime while system is running to change the handle
 */
signal_t timing_reinit(Timing_handle_t *handle, Process_control_block_t *timing_queue_processor, bool reset_state);


#endif /* _SYS_TIME_H_ */
