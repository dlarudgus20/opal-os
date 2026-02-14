#pragma once

#include <stddef.h>
#include <stdint.h>

struct singlylist_link {
    struct singlylist_link* next;
};

struct singlylist {
    struct singlylist_link dummy;
};

void singlylist_init(struct singlylist* list);
struct singlylist_link* singlylist_head(struct singlylist* list);
struct singlylist_link* singlylist_before_head(struct singlylist* list);

void singlylist_push_front(struct singlylist* restrict list, struct singlylist_link* restrict to_insert);
struct singlylist_link* singlylist_pop_front(struct singlylist* list);

void singlylist_insert_after(struct singlylist_link* restrict link, struct singlylist_link* restrict to_insert);
void singlylist_remove_after(struct singlylist_link* before);

#define singlylist_foreach(ptr, list) \
    for (struct singlylist_link* ptr = singlylist_head(list); ptr != NULL; ptr = ptr->next)

#define singlylist_foreach_2(before, ptr, list) \
    for (struct singlylist_link *before = singlylist_before_head(list), *ptr = singlylist_head(list); \
        ptr != NULL; before = ptr, ptr = ptr->next)
