#ifndef OPAL_TASK_COROUTINE_H
#define OPAL_TASK_COROUTINE_H

#include <kc/stdlib.h>

#include <collections/linkedlist.h>

struct coroutine;

enum coroutine_state {
    CO_DEFERRED,
    CO_READY,
    CO_DONE,
};

typedef enum coroutine_state co_state_t;
typedef co_state_t (*co_handler_t)(struct coroutine *co);

struct coroutine {
    struct linkedlist_link link;
    co_handler_t handler;
    co_state_t state;
};

void coroutine_worker_init(void);

void coroutine_init(struct coroutine *co, co_handler_t handler);
void coroutine_set_ready(struct coroutine *co);

#endif
