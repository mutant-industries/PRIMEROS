// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <collection/list.h>
#include <stddef.h>
#include <driver/interrupt.h>


void list_add_last(List_item_t *item, List_item_t **list) {

    interrupt_suspend();

    // list empty
    if ( ! *list) {
        *list = item;
        item->_next = item;
        item->_prev = item;
    }
    // place item to the end of list
    else {
        item->_next = *list;
        item->_prev = (*list)->_prev;
        (*list)->_prev->_next = item;
        (*list)->_prev = item;
    }

    interrupt_restore();
}

void list_remove(List_item_t *item, List_item_t **list) {

    interrupt_suspend();

    if ( ! list) {
        // item does not belong to any list, nothing to be done
    }
    else if (*list == item && (item->_next == item || ! item->_prev && ! item->_next)) {
        // last item on list
        *list = NULL;
    }
    else {
        if (item->_prev) {
            item->_prev->_next = item->_next;
        }

        if (item->_next) {
            item->_next->_prev = item->_prev;
        }

        if (*list == item) {
            *list = item->_next;
        }
    }

    // discard all ties to the list
    item->_next = item->_prev = NULL;

    interrupt_restore();
}
