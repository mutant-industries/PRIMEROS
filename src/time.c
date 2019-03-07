// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <time.h>
#include <driver/interrupt.h>
#include <action.h>
#include <action/queue.h>
#include <compiler.h>
#include <process.h>


// -------------------------------------------------------------------------------------

#ifndef __TIMING_QUEUE_HANDLER_PRIORITY__
#define __TIMING_QUEUE_HANDLER_PRIORITY__           ((uint16_t) (0xFF00))
#endif

// -------------------------------------------------------------------------------------

// timer driver handle
static Timing_handle_t *_timing_handle;

// timed signal queue, sorted by priority desc, _unsorted_queue_handler inherits it's priority
__persistent static Action_queue_t _unsorted_signal_queue = {0};
// timed signal, sorted by _trigger_time in ascending order
__persistent static Action_queue_t _upcoming_signal_queue = {0};

// signal triggered when new timed signal schedule request is created
__persistent static Action_signal_t _unsorted_queue_handler = {0};
// signal triggered when (one or more) upcoming signal(s) should be triggered
__persistent static Action_signal_t _upcoming_queue_handler = {0};

// absolute time that corresponds to _timing_handle->_timer_counter_last_stable
static Time_unit_t _current_time_last_stable;

// time tracking request count (user requests + unhandled periodic signals count)
static uint16_t _track_time_request_cnt;


// -------------------------------------------------------------------------------------
// time conversion, time unit manipulation

#define HOUR_SECONDS        ((uint32_t) (3600))
#define HOUR_MICROSECONDS   ((uint32_t) (HOUR_SECONDS * 1000 * 1000))

#ifdef __TIMING_CONVERSION_AVOID_HW_MULTIPLICATION__

/**
 * Return input value multiplied by 1000 (s -> ms, ms -> us) using bit shifts
 *  - "value * 1024 - value * 32 + value * 8" optimized for 16-bit instruction set ( ~ 36 instructions)
 *  - 'value' max is 4_294_967 = 0x418937, unchecked
 */
static uint32_t _mul_1000(uint32_t value) {
    uint32_t result, intermediate_result;

    if ( ! value) {
        return 0;
    }

    ((uint16_t *) &intermediate_result)[0] = ((uint16_t *) &value)[0] << 10;
    ((uint16_t *) &intermediate_result)[1] = ((uint16_t *) &value)[0] >> 6;
    ((uint8_t *) &intermediate_result)[3] |= ((uint8_t *) &value)[2] << 2;

    result = intermediate_result;

    ((uint16_t *) &intermediate_result)[0] = ((uint16_t *) &value)[0] << 5;
    ((uint16_t *) &intermediate_result)[1] = ((uint16_t *) &value)[1] << 5;
    ((uint8_t *) &intermediate_result)[2] |= ((uint8_t *) &value)[1] >> 3;

    result -= intermediate_result;

    ((uint16_t *) &intermediate_result)[0] = ((uint16_t *) &value)[0] << 3;
    ((uint16_t *) &intermediate_result)[1] = ((uint16_t *) &value)[1] << 3;
    ((uint8_t *) &intermediate_result)[2] |= ((uint8_t *) &value)[1] >> 5;

    result += intermediate_result;

    return result;
}

#endif

static void _time_unit_add_usecs(Time_unit_t *target, uint32_t usecs) {

    // check whether 32-bit usecs going to overflow
    if (target->usecs > UINT32_MAX - usecs) {
        target->hrs++;
    }

    target->usecs += usecs;

    // usecs never exceeds HOUR_MICROSECONDS
    if (target->usecs >= HOUR_MICROSECONDS) {
        target->hrs++;
        // max once
        target->usecs -= HOUR_MICROSECONDS;
    }
}

static uint32_t _no_conversion(uint32_t value) {
    return value;
}

Time_unit_t * time_unit_from(Time_unit_t *time_unit, uint16_t hrs, uint16_t secs, uint16_t millisecs, uint32_t usecs) {
    time_unit->hrs = hrs;

#ifdef __TIMING_CONVERSION_AVOID_HW_MULTIPLICATION__
    time_unit->usecs = _mul_1000(_mul_1000(secs) + millisecs) + usecs;
#else
    time_unit->usecs = (((uint32_t) ((((uint32_t) secs) * 1000) + millisecs)) * 1000) + usecs;
#endif

    if (time_unit->usecs > HOUR_MICROSECONDS) {
        time_unit->hrs++;
        // max once
        time_unit->usecs -= HOUR_MICROSECONDS;
    }

    return time_unit;
}

// -------------------------------------------------------------------------------------

static void _timing_restart() {

    interrupt_suspend();

    timer_channel_start(_timing_handle);
    // expect that timer counter can contain any random value now
    timer_channel_get_counter(_timing_handle, &_timing_handle->_timer_counter_last_stable);
    // set next interrupt in next static increment
    timer_channel_set_compare_value(_timing_handle, (_timing_handle->_timer_counter_last_stable
                    + _timing_handle->_timer_overflow_ticks_increment) & _timing_handle->_timer_counter_mask);
    // clear interrupt in case triggered within this function
    vector_clear_interrupt_flag(_timing_handle);

    // current time reset
    time_unit_reset(&_current_time_last_stable);

    interrupt_restore();
}

bool get_current_time(Time_unit_t *target) {
    // increment since last stable value
    uint32_t timer_counter_increment;
    // must be set to zero since timer handle might only set lower 16 bits
    uint32_t current_timer_counter_read = 0;

    if ( ! timer_channel_is_active(_timing_handle)) {
        return false;
    }

    interrupt_suspend();

    timer_channel_get_counter(_timing_handle, &current_timer_counter_read);

    timer_counter_increment = (current_timer_counter_read - _timing_handle->_timer_counter_last_stable) & _timing_handle->_timer_counter_mask;

    // see if _current_time_last_stable has to get static increment
    if (timer_counter_increment >= _timing_handle->_timer_overflow_ticks_increment) {
        // add static increment to current time
        _time_unit_add_usecs(&_current_time_last_stable, _timing_handle->_timer_overflow_us_increment);
        // store last (stable) read value
        _timing_handle->_timer_counter_last_stable = (_timing_handle->_timer_counter_last_stable + _timing_handle->_timer_overflow_ticks_increment) & _timing_handle->_timer_counter_mask;
        // set timer to next increment
        timer_channel_set_compare_value(_timing_handle, (_timing_handle->_timer_counter_last_stable + _timing_handle->_timer_overflow_ticks_increment) & _timing_handle->_timer_counter_mask);

        timer_counter_increment -= _timing_handle->_timer_overflow_ticks_increment;
    }

    if (target) {
        // copy stable value to target...
        time_unit_copy(&_current_time_last_stable, target);
        // ...and add actual difference
        _time_unit_add_usecs(target, _timing_handle->ticks_to_usecs(timer_counter_increment));
    }

    interrupt_restore();

    return true;
}

void set_track_current_time(bool track) {

    interrupt_suspend();

    if (track) {
        if ( ! timer_channel_is_active(_timing_handle)) {
            _timing_restart();
        }

        _track_time_request_cnt++;
    }
    else {
        _track_time_request_cnt--;
    }

    interrupt_restore();
}

bool get_upcoming_event_time(Time_unit_t *target) {
    bool result;

    interrupt_suspend();

    if ((result = ! action_queue_is_empty(&_upcoming_signal_queue))) {
        // copy time of next upcoming signal to target
        time_unit_copy(timed_signal_trigger_time(action_queue_head(&_upcoming_signal_queue)), target);
    }

    interrupt_restore();

    return result;
}

// -------------------------------------------------------------------------------------

static bool _timer_increment_compare_value(Timing_handle_t *handle, uint32_t increment) {
    // interrupts are disabled already

    bool result = true;

    uint32_t current_timer_counter_read = 0;
    uint32_t next_timer_compare_value = handle->_timer_counter_last_stable + increment;

    timer_channel_get_counter(handle, &current_timer_counter_read);
    uint32_t ticks_increment = (next_timer_compare_value - current_timer_counter_read) & handle->_timer_counter_mask;

    /**
     * ticks_increment < compare_value_set_threshold -> next_timer_compare_value is too close (is already history when set)
     * ticks_increment > overflow_ticks_increment -> next_timer_compare_value is history (precedes current_timer_counter_read)
     */
    if (ticks_increment < handle->_timer_compare_value_set_threshold || ticks_increment > handle->_timer_overflow_ticks_increment) {
        // next compare at next stable increment
        next_timer_compare_value = handle->_timer_counter_last_stable + handle->_timer_overflow_ticks_increment;

        // check whether given increment is not too close to next stable increment
        if (handle->_timer_overflow_ticks_increment - increment < handle->_timer_compare_value_set_threshold) {
            next_timer_compare_value += handle->_timer_compare_value_set_threshold;
        }

        result = false; // given increment could not be set, the closest stable increment was set instead
    }

    // set result timer compare value
    timer_channel_set_compare_value(handle, next_timer_compare_value & handle->_timer_counter_mask);
    // clear interrupt in case triggered while interrupts are disabled
    vector_clear_interrupt_flag(handle);

    return result;
}

static bool _check_upcoming_signal_queue(bool trigger_upcoming_queue_handler) {
    // interrupts are disabled already

    // static - stack usage optimization
    static Time_unit_t current_time;

    // assert upcoming signal queue is not empty
    Time_unit_t *head_trigger_time = timed_signal_trigger_time(action_queue_head(&_upcoming_signal_queue));

    // assert timing is active when this point reached (upcoming signal queue is not empty)
    get_current_time(&current_time);

    if (head_trigger_time->hrs < current_time.hrs
            || head_trigger_time->hrs <= current_time.hrs && head_trigger_time->usecs <= current_time.usecs) {

        if (trigger_upcoming_queue_handler) {
            // just keep time tracking running...
            _timer_increment_compare_value(_timing_handle, _timing_handle->_timer_overflow_ticks_increment);
            // ...and trigger upcoming queue handler signal
            action_trigger(&_upcoming_queue_handler, NULL);
        }
        else {
            // signal that has trigger_time <= current_time
            Timed_signal_t *signal = timed_signal(action_queue_head(&_upcoming_signal_queue));

            // create time tracking request if signal is periodic
            if (signal->_periodic) {
                // create time tracking request
                _track_time_request_cnt++;
            }

            // just trigger signal, timer compare value shall be changed within next call (if required)
            action_trigger(signal, TIMING_SIGNAL_TIMEOUT);
        }

        return true;    // upcoming event queue might contain more signals that should be triggered
    }
    else if (head_trigger_time->hrs == current_time.hrs
             || head_trigger_time->hrs == current_time.hrs + 1 && head_trigger_time->usecs < current_time.usecs) {

        // in how many usecs is next upcoming signal going to be triggered
        uint32_t upcoming_signal_in_usecs = head_trigger_time->usecs - _current_time_last_stable.usecs;

        // overflow usecs - when head_trigger_time->hrs == current_time.hrs + 1 && head_trigger_time->usecs < current_time.usecs
        if (head_trigger_time->usecs < _current_time_last_stable.usecs) {
            upcoming_signal_in_usecs += HOUR_MICROSECONDS;
        }

        // see if upcoming signal fits to next CCR increment - if so try to set timer compare value
        if (upcoming_signal_in_usecs <= _timing_handle->_timer_overflow_us_increment
                && ! _timer_increment_compare_value(_timing_handle, _timing_handle->usecs_to_ticks(upcoming_signal_in_usecs))) {

            if (trigger_upcoming_queue_handler) {
                action_trigger(&_upcoming_queue_handler, NULL);
            }

            return true;    // upcoming event is too close, timer was set to next stable value
        }
    }

    return false;   // no more upcoming signals to be processed
}

static void _timing_handle_service() {
    // interrupt service routine, assume interrupts are disabled already

    if (action_queue_is_empty(&_upcoming_signal_queue) && action_queue_is_empty(&_unsorted_signal_queue)
            && ! _track_time_request_cnt) {
        // no more timed signals and no time tracking, timer can be stopped
        timer_channel_stop(_timing_handle);
    }
    else if (action_queue_is_empty(&_upcoming_signal_queue)) {
        // just keep track of current time, no need to get exact 'now', assert timing is active when this point reached
        get_current_time(NULL);
    }
    else {
        _check_upcoming_signal_queue(true);
    }
}

// -------------------------------------------------------------------------------------

/**
 * executed once per each insert to _unsorted_signal_queue with priority inherited from _unsorted_signal_queue.head
 */
static bool _unsorted_queue_handle() {
    Timed_signal_t *head, *current;

    interrupt_suspend();

    if (head = timed_signal(action_queue_pop(&_unsorted_signal_queue))) {

        current = timed_signal(action_queue_head(&_upcoming_signal_queue));

        if ( ! current || timed_signal_trigger_time(current)->hrs > timed_signal_trigger_time(head)->hrs
             || (timed_signal_trigger_time(current)->hrs == timed_signal_trigger_time(head)->hrs
                 && timed_signal_trigger_time(current)->usecs >= timed_signal_trigger_time(head)->usecs)) {

            // queue empty or closest upcoming signal
            deque_insert_first(deque(&_upcoming_signal_queue), deque_item(head));

            // see whether upcoming signal handler should be triggered
            _check_upcoming_signal_queue(true);
        }
        else if (timed_signal_trigger_time(deque_item_prev(current))->hrs < timed_signal_trigger_time(head)->hrs
                 || (timed_signal_trigger_time(deque_item_prev(current))->hrs == timed_signal_trigger_time(head)->hrs
                     && timed_signal_trigger_time(deque_item_prev(current))->usecs <= timed_signal_trigger_time(head)->usecs)) {

            // farthermost upcoming signal
            deque_insert_last(deque(&_upcoming_signal_queue), deque_item(head));
        }
        else {
            current = timed_signal(deque_item_next(action_queue_head(&_upcoming_signal_queue)));

            // place before first signal that is going to be triggered
            while ((timed_signal_trigger_time(current)->hrs < timed_signal_trigger_time(head)->hrs
                    || timed_signal_trigger_time(current)->hrs <= timed_signal_trigger_time(head)->hrs
                       && timed_signal_trigger_time(current)->usecs < timed_signal_trigger_time(head)->usecs)
                   && deque_item_next(current) != deque_item(action_queue_head(&_upcoming_signal_queue))) {

                current = timed_signal(deque_item_next(current));
            }

            deque_insert_before(deque_item(head), deque_item(current));
        }
    }

    interrupt_restore();

    return true;
}

static void _hours_overflow_adjust(Action_queue_t *queue) {
    // interrupts are disabled already
    Timed_signal_t *current;

    if (action_queue_is_empty(queue)) {
        return;
    }

    current = timed_signal(action_queue_head(queue));

    while (true) {
        timed_signal_trigger_time(current)->hrs &= ~0x8000;

        if ((current = timed_signal(deque_item_next(current))) == timed_signal(action_queue_head(queue))) {
            break;
        }
    }
}

/**
 * executed if _upcoming_queue_handler signal was triggered within _check_upcoming_signal_queue,
 * execution priority is static (__TIMING_QUEUE_HANDLER_PRIORITY__)
 */
static bool _upcoming_queue_handle() {
    bool upcoming_events_pending = true;

    while (upcoming_events_pending) {

        interrupt_suspend();

        // mo more than one check with disabled interrupts
        upcoming_events_pending =  ! action_queue_is_empty(&_upcoming_signal_queue) && _check_upcoming_signal_queue(false);

        interrupt_restore();
    }

    interrupt_suspend();

    // check whether stable time reached half of total range
    if (_current_time_last_stable.hrs & 0x8000) {
        _current_time_last_stable.hrs &= ~0x8000;
        // ... if so then trigger_time of all scheduled timed signals has to be also adjusted
        _hours_overflow_adjust(&_upcoming_signal_queue);
        _hours_overflow_adjust(&_unsorted_signal_queue);
    }

    interrupt_restore();

    return true;
}

// -------------------------------------------------------------------------------------

static signal_t _set_periodic(Timed_signal_t *_this, bool periodic) {

    interrupt_suspend();

    if (periodic != _this->_periodic) {
        // periodic state change affects time tracking state if signal was triggered and handler was not yet processed
        if (action_queue(deque_item_container(_this)) == &action_signal_execution_context(_this)->pending_signal_queue) {
            // create / remove time tracking request
            set_track_current_time(periodic);
        }

        _this->_periodic = periodic;
    }

    interrupt_restore();

    return TIMING_SUCCESS;
}

static signal_t _schedule(Timed_signal_t *_this) {

    // sanity check
    if ( ! _timing_handle) {
        return TIMING_INVALID_STATE;
    }

    interrupt_suspend();

    if ( ! timer_channel_is_active(_timing_handle)) {
        // start timing,
        _timing_restart();
        // reset trigger time in case signal reused
        time_unit_reset(timed_signal_trigger_time(_this));
    }
    else {
        // make trigger time match current time
        get_current_time(timed_signal_trigger_time(_this));
    }

    timed_signal_trigger_time(_this)->hrs += timed_signal_delay(_this)->hrs;
    _time_unit_add_usecs(timed_signal_trigger_time(_this), timed_signal_delay(_this)->usecs);

    // insert to unsorted queue, also remove time tracking request if periodic (in _on_timed_signal_released hook)
    action_queue_insert(&_unsorted_signal_queue, _this);
    action_trigger(&_unsorted_queue_handler, NULL);

    interrupt_restore();

    return TIMING_SUCCESS;
}

// -------------------------------------------------------------------------------------

static void _on_timed_signal_released(Timed_signal_t *_this, Action_queue_t *origin) {
    // interrupts are disabled already

    if ( ! _this->_periodic || origin != &action_signal_execution_context(_this)->pending_signal_queue) {
        return;
    }

    // periodic signal was released from target context, it no longer needs time tracking
    _track_time_request_cnt--;
}

static bool _on_timed_signal_handled(Timed_signal_t *_this) {
    // interrupts are disabled already

    if ( ! _this->_periodic) {
        return false;
    }

    // update signal trigger time
    timed_signal_trigger_time(_this)->hrs += timed_signal_delay(_this)->hrs;
    _time_unit_add_usecs(timed_signal_trigger_time(_this), timed_signal_delay(_this)->usecs);

    // current time overflow check
    if (timed_signal_trigger_time(_this)->hrs & 0x8000
            && timed_signal_trigger_time(_this)->hrs - _current_time_last_stable.hrs > MAX_DELAY_HRS) {
        timed_signal_trigger_time(_this)->hrs &= ~0x8000;
    }

    // reinsert to unsorted queue, also remove time tracking request if periodic (in _on_timed_signal_released hook)
    action_queue_insert(&_unsorted_signal_queue, _this);
    action_trigger(&_unsorted_queue_handler, NULL);

    return true;
}

// Timed_signal_t destructor
static dispose_function_t _timed_signal_dispose(Timed_signal_t *_this) {

    _this->schedule = (signal_t (*)(Timed_signal_t *)) unsupported_after_disposed;
    _this->set_periodic = (signal_t (*)(Timed_signal_t *, bool)) unsupported_after_disposed;

    return NULL;
}

// Timed_signal_t constructor
void timed_signal_register(Timed_signal_t *signal, signal_handler_t handler, bool periodic,
        Schedule_config_t *with_config, Process_control_block_t *context) {

    // parent constructor
    action_signal_create(signal, (dispose_function_t) _timed_signal_dispose, handler, with_config, context);
    // set default on_handled hook
    action_signal_on_handled(signal) = (bool (*)(Action_signal_t *)) _on_timed_signal_handled;
    // set default on_released hook to ensure _track_time_request_cnt is consistent
    action_on_released(signal) = (action_released_hook_t) _on_timed_signal_released;

    // state
    signal->_periodic = periodic;

    // public
    signal->set_periodic = _set_periodic;
    signal->schedule = _schedule;
}

// -------------------------------------------------------------------------------------

signal_t timing_reinit(Timing_handle_t *handle, Process_control_block_t *timing_queue_processor, bool persistent_state_reset) {

    // sanity check
    if ( ! handle) {
        return TIMING_HANDLE_EMPTY;
    }
    else if (handle->timer_counter_bit_width < 8 || handle->timer_counter_bit_width > 32) {
        return TIMING_HANDLE_UNSUPPORTED;
    }

    // try register handle interrupt service handler
    if (timer_channel_stop(handle) || vector_clear_interrupt_flag(handle) || vector_set_enabled(handle, true)
            || vector_register_handler(handle, _timing_handle_service, NULL, NULL) == NULL) {

        return TIMING_HANDLE_UNSUPPORTED;
    }

    if (_timing_handle && persistent_state_reset) {
        timer_channel_stop(_timing_handle);
    }

    // first time initialization / reset
    if (persistent_state_reset) {
        // unsorted queue handler
        action_signal_create(&_unsorted_queue_handler, NULL, _unsorted_queue_handle, NULL, timing_queue_processor);
        // priority of unsorted queue handler is inherited only from unsorted queue head (not from actual running process)
        sorted_set_item_priority(&_unsorted_queue_handler) = 0;
        // upcoming signal queue handler
        action_signal_create(&_upcoming_queue_handler, NULL, _upcoming_queue_handle, NULL, timing_queue_processor);
        // priority of upcoming queue handler is static (not inherited from actual running process)
        sorted_set_item_priority(&_upcoming_queue_handler) = __TIMING_QUEUE_HANDLER_PRIORITY__;
        // unsorted_queue_handler - inherit priority of unsorted_signal_queue
        action_queue_create(&_unsorted_signal_queue, true, false, &_unsorted_queue_handler, action_default_set_priority);
        // upcoming signals
        action_queue_create(&_upcoming_signal_queue, false);
        // no tracking time on reset, no periodic signals active
        _track_time_request_cnt = 0;
    }

    // default if timer clocked by 1MHz -> 1 tick == 1 us
    if ( ! handle->ticks_to_usecs || ! handle->usecs_to_ticks) {
        handle->ticks_to_usecs = handle->usecs_to_ticks = _no_conversion;
    }

    handle->_timer_counter_mask = UINT32_MAX;

    if (handle->timer_counter_bit_width < 32) {
        handle->_timer_counter_mask = ~(handle->_timer_counter_mask << handle->timer_counter_bit_width);
    }

    // The following code is here just to make life easier and support most timers out of the box with
    // minimal (or zero) timing error. It actually does not ensure optional configuration. The goal:
    // - _timer_overflow_ticks_increment should be roughly 75% - 90% of _timer_counter_mask
    // - _timer_overflow_ticks_increment should be some number, that is convertible to usecs with zero error

    // increment of timer compare register - assume timer interrupt is always processed within (full_range - increment) ticks
    //  - used constants are just estimate of what might fit most ticks->us conversions
    handle->_timer_overflow_ticks_increment = (uint32_t) (handle->timer_counter_bit_width >= 24 ?
          // increment divisible by 5^3, 3^3 and at least by 2^12
          0xD2F000 << (handle->timer_counter_bit_width - 24) : handle->timer_counter_bit_width >= 16 ?
          // increment divisible by 5^2, 3^2 and at least by 2^8
          0xE100 << (handle->timer_counter_bit_width - 16) :
          // increment divisible by 5, 3 and at least by 2^4 (timer clock max = CPU clock / 256)
          0xF0 << (handle->timer_counter_bit_width - 8));

    // there should be no error (remainder) in this conversion - if not then expect this error on each timer register increment
    handle->_timer_overflow_us_increment = handle->ticks_to_usecs(handle->_timer_overflow_ticks_increment);

    // if 1 tick is more that 1 us (_timer_overflow_us_increment overflow check)
    if (handle->ticks_to_usecs(0x2A30) > 0x2A30) {
        uint32_t timer_overflow_ticks_max;

        // actual maximum of _timer_overflow_us_increment larger than absolute maximum
        if (handle->_timer_overflow_ticks_increment > (timer_overflow_ticks_max = handle->usecs_to_ticks(HOUR_MICROSECONDS))) {
            // HOUR_MICROSECONDS is divisible by 5^8, 3^2 and 2^10, therefore the conversion error should not be too bad
            handle->_timer_overflow_ticks_increment = timer_overflow_ticks_max;
            // ... and the absolute error is added to current time just once per hour
            handle->_timer_overflow_us_increment = HOUR_MICROSECONDS;
        }
    }

    interrupt_suspend();

    // timer counter read-write minimal ticks threshold (worst case) setup
    // - it takes some time to read timer counter, compute next compare value and write that value to compare register
    // - the compare register must be always set to 'future' value
    // - minimal threshold defines minimal compare value increment of compare register and is defined by number of
    // timer ticks it takes to process _timer_increment_compare_value() function
    handle->_timer_compare_value_set_threshold = 0;

    if (vector_set_enabled(handle, false) || timer_channel_start(handle)
            || timer_channel_get_counter(handle, &handle->_timer_counter_last_stable)) {

        return TIMING_HANDLE_UNSUPPORTED;
    }

    // threshold = number of timer ticks it takes to process _timer_increment_compare_value()
    _timer_increment_compare_value(handle, handle->_timer_overflow_ticks_increment);

    if (timer_channel_get_counter(handle, &handle->_timer_compare_value_set_threshold) || timer_channel_stop(handle)
            || vector_set_enabled(handle, true)) {

        return TIMING_HANDLE_UNSUPPORTED;
    }

    handle->_timer_compare_value_set_threshold = (handle->_timer_compare_value_set_threshold
            - handle->_timer_counter_last_stable) & handle->_timer_counter_mask;

    // threshold must be always at least 1
    if ( ! handle->_timer_compare_value_set_threshold) {
        handle->_timer_compare_value_set_threshold = 1;
    }

    if ( ! persistent_state_reset && _timing_handle && timer_channel_is_active(_timing_handle)) {
        // start new handle...
        timer_channel_start(handle);
        // ...and store it's counter content
        timer_channel_get_counter(handle, &handle->_timer_counter_last_stable);
        // sync &handle->_timer_counter_last_stable and &_current_time_last_stable
        get_current_time(&_current_time_last_stable);
        // stop previous handle
        timer_channel_stop(_timing_handle);
        // set next interrupt in next static increment
        timer_channel_set_compare_value(handle, (handle->_timer_counter_last_stable
                + handle->_timer_overflow_ticks_increment) & handle->_timer_counter_mask);
        // clear interrupt in case triggered within this function
        vector_clear_interrupt_flag(handle);

        // let upcoming queue handler set proper next timer compare value
        if ( ! action_queue_is_empty(&_upcoming_signal_queue)) {
            action_trigger(&_upcoming_queue_handler, NULL);
        }
    }

    _timing_handle = handle;

    // handle API return values ignored from now on, assume handle is not disposed

    interrupt_restore();

    return TIMING_SUCCESS;
}
