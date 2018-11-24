// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2018-2019 Mutant Industries ltd.
#include <collection/list.h>
#include <stddef.h>
#include <driver/interrupt.h>


void list_add_last(List_item_t **list, List_item_t *item) {

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

void list_remove(List_item_t **list, List_item_t *item) {

    interrupt_suspend();

    // last item on list
    if (item->_next == item && *list == item) {
        *list = NULL;
    }
    else {
        item->_prev->_next = item->_next;
        item->_next->_prev = item->_prev;

        if (*list == item) {
            *list = item->_next;
        }
    }

    // discard all ties to the list
    item->_next = item->_prev = item;

    interrupt_restore();
}
