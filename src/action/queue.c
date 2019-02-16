// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <action/queue.h>
#include <stddef.h>
#include <driver/interrupt.h>


// move queue iterator to next position after current or set NULL when current->next is queue head
#define _iterator_advance(_queue) (_queue)->_iterator = action(deque_item_next((_queue)->_iterator)) == (_queue)->_head ? \
                    NULL : action(deque_item_next((_queue)->_iterator));

// -------------------------------------------------------------------------------------

static Action_t *_pop_unsafe(Action_queue_t *_this) {
    Action_t *head = action(sorted_set_poll_last(sorted_set(_this)));

    if (head && action_on_released(head)) {
        action_released_callback(head, _this);
    }

    return head;
}

static Action_t *_pop(Action_queue_t *_this) {
    Action_t *head;

    interrupt_suspend();

    if (_this->_head && _this->_iterator == _this->_head) {
        _iterator_advance(_this);
    }

    head = _pop_unsafe(_this);

    interrupt_restore();

    return head;
}

static Action_t *_pop_sorted(Action_queue_t *_this) {
    Action_t *head;

    interrupt_suspend();

    if (_this->_head && _this->_iterator == _this->_head) {
        _iterator_advance(_this);
    }

    head = _pop_unsafe(_this);

    if ((action_queue_is_empty(_this) && _this->_head_priority) || ( ! action_queue_is_empty(_this)
                && sorted_set_item_priority(action_queue_head(_this)) != _this->_head_priority)) {

        _this->_head_priority = action_queue_is_empty(_this) ? 0 : sorted_set_item_priority(action_queue_head(_this));

        if (_this->_on_head_priority_changed) {
            _this->_on_head_priority_changed(_this->_owner, _this->_head_priority, _this);
        }
    }

    interrupt_restore();

    return head;
}

// -------------------------------------------------------------------------------------
// assume interrupts are disabled already, assume action belongs to some queue

static void _release(Action_t *action) {
    Action_queue_t *queue = action_queue(deque_item_container(action));

    if (queue->_iterator == action) {
        _iterator_advance(queue);
    }

    deque_item_remove(deque_item(action));

    if (action_on_released(action)) {
        action_released_callback(action, queue);
    }
}

static void _release_sorted(Action_t *action) {
    Action_queue_t *queue = action_queue(deque_item_container(action));

    if (queue->_iterator == action) {
        _iterator_advance(queue);
    }

    deque_item_remove(deque_item(action));

    if (action_on_released(action)) {
        action_released_callback(action, queue);
    }

    // removed last action from queue, head priority changed
    if ((action_queue_is_empty(queue) && queue->_head_priority) || ( ! action_queue_is_empty(queue)
                && sorted_set_item_priority(action_queue_head(queue)) != queue->_head_priority)) {

        queue->_head_priority = action_queue_is_empty(queue) ? 0 : sorted_set_item_priority(action_queue_head(queue));

        if (queue->_on_head_priority_changed) {
            queue->_on_head_priority_changed(queue->_owner, queue->_head_priority, queue);
        }
    }
}

// -------------------------------------------------------------------------------------
// assume interrupts are disabled already, assume action belongs to given queue

static bool _set_priority(Action_t *action, priority_t priority, Action_queue_t *_) {
    // no sorting, just update the priority
    sorted_set_item_priority(action) = priority;

    return false;
}

static bool _set_priority_sorted_unsafe(Action_t *action, priority_t priority, Action_queue_t *_this) {
    bool highest_priority_placement;

    highest_priority_placement = sorted_set_item_set_priority(sorted_set_item(action), priority);

    if (sorted_set_item_priority(action_queue_head(_this)) != _this->_head_priority) {
        _this->_head_priority = sorted_set_item_priority(action_queue_head(_this));

        if (_this->_on_head_priority_changed) {
            _this->_on_head_priority_changed(_this->_owner, _this->_head_priority, _this);
        }
    }

    return highest_priority_placement;
}

static bool _set_priority_sorted(Action_t *action, priority_t priority, Action_queue_t *_this) {
    bool head_priority_changed = false;

    if ( ! _this->_iterator) {
        // trigger_all() is not running, standard behavior
        return _set_priority_sorted_unsafe(action, priority, _this);
    }

    // if action does not remove itself during trigger then queue head priority becomes just a guess
    if (priority > _this->_head_priority) {
        _this->_head_priority = priority;

        head_priority_changed = true;
    }
    else if (priority < _this->_head_priority && action == action_queue_head(_this)
                && sorted_set_item_priority(deque_item_next(deque_item(action))) < _this->_head_priority) {
        _this->_head_priority = sorted_set_item_priority(deque_item_next(deque_item(action)));

        head_priority_changed = true;
    }

    // just update action priority without queue re-sorting
    sorted_set_item_priority(action) = priority;

    if (head_priority_changed && _this->_on_head_priority_changed) {
        _this->_on_head_priority_changed(_this->_owner, _this->_head_priority, _this);
    }

    return false;
}

static bool _set_priority_sorted_strict(Action_t *action, priority_t priority, Action_queue_t *_this) {

    if (_this->_iterator == action) {
        // trigger_all() is running, just advance iterator as if action was removed from queue
        _iterator_advance(_this);
    }

    // strict sorting - no changes in sorting behavior
    return _set_priority_sorted_unsafe(action, priority, _this);
}

// -------------------------------------------------------------------------------------

static bool _insert(Action_queue_t *_this, Action_t *action) {
    Action_queue_t *queue;

    interrupt_suspend();

    if ((queue = action_queue(deque_item_container(action)))) {
        queue->_release(action);
    }

    // thread-safety check
    if ( ! action_queue_is_closed(_this)) {
        deque_insert_last(deque(_this), deque_item(action));
    }

    interrupt_restore();

    return false;
}

static bool _insert_sorted(Action_queue_t *_this, Action_t *action) {
    bool highest_priority_placement = false;
    Action_queue_t *queue;

    interrupt_suspend();

    if ((queue = action_queue(deque_item_container(action)))) {
        queue->_release(action);
    }

    // thread-safety check
    if ( ! action_queue_is_closed(_this)) {
        highest_priority_placement = sorted_set_add(sorted_set(_this), sorted_set_item(action));

        if (sorted_set_item_priority(action_queue_head(_this)) != _this->_head_priority) {
            _this->_head_priority = sorted_set_item_priority(action_queue_head(_this));

            if (_this->_on_head_priority_changed) {
                _this->_on_head_priority_changed(_this->_owner, _this->_head_priority, _this);
            }
        }
    }

    interrupt_restore();

    return highest_priority_placement;
}

// -------------------------------------------------------------------------------------

static void _trigger_all(Action_queue_t *_this, signal_t signal) {
    Action_t *current;

    interrupt_suspend();

    // current queue head is now going to be triggered if set
    current = _this->_iterator = _this->_head;

    // prepare queue iterator if queue is not empty
    if (current) {
        _iterator_advance(_this);
    }

    interrupt_restore();

    while (current) {
        // execute action with given signal
        action_trigger(current, signal);

        interrupt_suspend();

        // move to next action in queue, current is now going to be triggered if set
        current = _this->_iterator;

        if (current) {
            _iterator_advance(_this);
        }

        interrupt_restore();
    }
}

static void _close(Action_queue_t *_this, signal_t signal) {
    Action_t *current;

    // disable insert
    _this->insert = (bool (*)(Action_queue_t *, Action_t *)) unsupported_after_disposed;
    // make last destructive trigger_all()

    while ((current = action_queue_head(_this))) {
        action_trigger(current, signal);

        interrupt_suspend();

        // make sure current action is no longer linked to this queue
        if (action_queue(deque_item_container(current)) == _this) {
            _this->_release(current);
        }

        interrupt_restore();
    }
}

// -------------------------------------------------------------------------------------

// Action_queue_t constructor
void action_queue_init(Action_queue_t *queue, bool sorted, bool strict_sorting, void *owner,
        head_priority_changed_hook_t on_head_priority_changed) {

    queue->_head = NULL;
    queue->_owner = owner;
    queue->_on_head_priority_changed = on_head_priority_changed;

    // state
    queue->_iterator = NULL;
    queue->_head_priority = 0;

    // protected
    queue->_release = sorted ? _release_sorted : _release;
    queue->_set_action_priority = sorted ? strict_sorting ? _set_priority_sorted_strict : _set_priority_sorted : _set_priority;

    // public
    queue->insert = sorted ? _insert_sorted : _insert;
    queue->pop = sorted ? _pop_sorted : _pop;
    queue->trigger_all = _trigger_all;
    queue->close = _close;
}
