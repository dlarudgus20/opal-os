#include <opal/task/task.h>
#include <opal/task/event.h>
#include <opal/task/coroutine.h>
#include <opal/locks/irqlock.h>

static struct linkedlist g_job_queue;
static struct event g_job_event;
static taskptr_t g_worker;

static void check_and_destroy(struct coroutine *co) {
    if (co->state == CO_DONE) {
        co->handler(co);
    }
}

static void worker(uintptr_t) {
    while (1) {
        event_wait(&g_job_event, TIMEOUT_INFINITY);

        irqlock_t lock = irqlock_acquire();
        while (1) {
            struct linkedlist_link *link = linkedlist_pop_front(&g_job_queue);
            if (!link) {
                break;
            }

            struct coroutine *co = container_of(link, struct coroutine, link);
            if (co->state != CO_READY) {
                check_and_destroy(co);
                continue;
            }

            irqlock_release(&lock);
            co_state_t state = co->handler(co);
            lock = irqlock_acquire();

            co->state = state;
            if (state == CO_READY) {
                linkedlist_push_back(&g_job_queue, &co->link);
            } else {
                check_and_destroy(co);
            }
        }
        irqlock_release(&lock);
    }
}

void coroutine_worker_init(void) {
    linkedlist_init(&g_job_queue);
    event_init(&g_job_event, true);
    g_worker = task_create(worker, 0, TASK_PRIORITY_KERNEL);
}

void coroutine_init(struct coroutine *co, co_handler_t handler) {
    co->handler = handler;
    co->state = CO_DEFERRED;
}

void coroutine_set_ready(struct coroutine *co) {
    irqlock_t lock = irqlock_acquire();
    co->state = CO_READY;
    linkedlist_push_back(&g_job_queue, &co->link);
    event_signal(&g_job_event);
    irqlock_release(&lock);
}
