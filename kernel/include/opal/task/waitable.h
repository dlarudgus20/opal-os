#ifndef OPAL_TASK_WAITABLE_H
#define OPAL_TASK_WAITABLE_H

#include <collections/linkedlist.h>

struct waitable {
    struct linkedlist wait_queue;
    bool triggered;
    bool reset;
};

static inline void waitable_init(struct waitable *obj, bool reset) {
    linkedlist_init(&obj->wait_queue);
    obj->triggered = false;
    obj->reset = reset;
}

void waitable_trigger(struct waitable *obj);

#endif
