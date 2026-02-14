#pragma once

#include <stdbool.h>

struct linkedlist_link {
    struct linkedlist_link* prev;
    struct linkedlist_link* next;
};

struct linkedlist {
    struct linkedlist_link dummy;
};

void linkedlist_init(struct linkedlist* list);
struct linkedlist_link* linkedlist_nil(struct linkedlist* list);
bool linkedlist_is_nil(struct linkedlist* list, struct linkedlist_link* link);
bool linkedlist_is_empty(struct linkedlist* list);
struct linkedlist_link* linkedlist_head(struct linkedlist* list);
struct linkedlist_link* linkedlist_tail(struct linkedlist* list);

void linkedlist_push_front(struct linkedlist* restrict list, struct linkedlist_link* restrict to_insert);
void linkedlist_push_back(struct linkedlist* restrict list, struct linkedlist_link* restrict to_insert);
struct linkedlist_link* linkedlist_pop_front(struct linkedlist* list);
struct linkedlist_link* linkedlist_pop_back(struct linkedlist* list);

void linkedlist_insert_before(struct linkedlist_link* restrict link, struct linkedlist_link* restrict to_insert);
void linkedlist_insert_after(struct linkedlist_link* restrict link, struct linkedlist_link* restrict to_insert);
void linkedlist_remove(struct linkedlist_link* link);

#define linkedlist_foreach(ptr, list) \
    for (struct linkedlist_link* ptr = linkedlist_head(list); !linkedlist_is_nil(list, ptr); ptr = ptr->next)

#define linkedlist_foreach_backward(ptr, list) \
    for (struct linkedlist_link* ptr = linkedlist_tail(list); !linkedlist_is_nil(list, ptr); ptr = ptr->prev)
