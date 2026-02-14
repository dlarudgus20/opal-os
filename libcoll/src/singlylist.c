#include <kc/assert.h>
#include "collections/singlylist.h"

void singlylist_init(struct singlylist* list) {
    list->dummy.next = NULL;
}

struct singlylist_link* singlylist_head(struct singlylist* list) {
    return list->dummy.next;
}

struct singlylist_link* singlylist_before_head(struct singlylist* list) {
    return &list->dummy;
}

void singlylist_push_front(struct singlylist* restrict list, struct singlylist_link* restrict to_insert) {
    singlylist_insert_after(&list->dummy, to_insert);
}

struct singlylist_link* singlylist_pop_front(struct singlylist* list) {
    struct singlylist_link* removed = list->dummy.next;
    if (removed == NULL) {
        return NULL;
    }
    singlylist_remove_after(&list->dummy);
    return removed;
}

void singlylist_insert_after(struct singlylist_link* restrict link, struct singlylist_link* restrict to_insert) {
    to_insert->next = link->next;
    link->next = to_insert;
}

void singlylist_remove_after(struct singlylist_link* before) {
    assert(before->next != NULL, "singlylist: remove_after on tail");
    before->next = before->next->next;
}
