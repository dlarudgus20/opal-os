#include "collections/linkedlist.h"
#include <stddef.h>

void linkedlist_init(struct linkedlist* list) {
    list->dummy.prev = &list->dummy;
    list->dummy.next = &list->dummy;
}

struct linkedlist_link* linkedlist_nil(struct linkedlist* list) {
    return &list->dummy;
}

bool linkedlist_is_nil(struct linkedlist* list, struct linkedlist_link* link) {
    return link == &list->dummy;
}

bool linkedlist_is_empty(struct linkedlist* list) {
    return list->dummy.prev == &list->dummy && list->dummy.prev == list->dummy.next;
}

struct linkedlist_link* linkedlist_head(struct linkedlist* list) {
    return list->dummy.next;
}

struct linkedlist_link* linkedlist_tail(struct linkedlist* list) {
    return list->dummy.prev;
}

void linkedlist_push_front(struct linkedlist* restrict list, struct linkedlist_link* restrict to_insert) {
    linkedlist_insert_before(list->dummy.next, to_insert);
}

void linkedlist_push_back(struct linkedlist* restrict list, struct linkedlist_link* restrict to_insert) {
    linkedlist_insert_after(list->dummy.prev, to_insert);
}

struct linkedlist_link* linkedlist_pop_front(struct linkedlist* list) {
    struct linkedlist_link* removed = list->dummy.next;
    if (removed == &list->dummy) {
        return NULL;
    }
    linkedlist_remove(list->dummy.next);
    return removed;
}

struct linkedlist_link* linkedlist_pop_back(struct linkedlist* list) {
    struct linkedlist_link* removed = list->dummy.prev;
    if (removed == &list->dummy) {
        return NULL;
    }
    linkedlist_remove(list->dummy.prev);
    return removed;
}

void linkedlist_insert_before(struct linkedlist_link* restrict link, struct linkedlist_link* restrict to_insert) {
    link->prev->next = to_insert;
    to_insert->prev = link->prev;
    to_insert->next = link;
    link->prev = to_insert;
}

void linkedlist_insert_after(struct linkedlist_link* restrict link, struct linkedlist_link* restrict to_insert) {
    link->next->prev = to_insert;
    to_insert->next = link->next;
    to_insert->prev = link;
    link->next = to_insert;
}

void linkedlist_remove(struct linkedlist_link* link) {
    link->prev->next = link->next;
    link->next->prev = link->prev;
}
