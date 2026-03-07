#include <limits.h>

#include <kc/inttypes.h>
#include <kc/assert.h>
#include <kc/string.h>

#include <opal/irq.h>
#include <opal/timer.h>
#include <opal/task/task.h>
#include <opal/task/waitable.h>
#include <opal/mm/mm.h>
#include <opal/mm/slab.h>
#include <opal/locks/irqlock.h>
#include <opal/platform/asm.h>

static int tid_compare(struct task *a, struct task *b) {
    return a->id - b->id;
}

static int tid_compare_to(struct task *a, tid_t b) {
    return a->id - b;
}

static int timeout_compare(struct task *a, struct task *b) {
    if (a->wait_timeout > b->wait_timeout) {
        return 1;
    } else if (a->wait_timeout < b->wait_timeout) {
        return -1;
    } else {
        return a->id - b->id;
    }
}

RBTREE_TEMPLATE(struct task, tid_t, tid_node, tid_compare, tid_compare_to, task, static)
RBTREE_INSERT_TEMPLATE(struct task, timeout_node, timeout_compare, timeout, static)

struct sched {
    tid_t tid_next;
    struct rbtree tid_tree;
    struct slab task_slab;

    struct linkedlist ready_queue;
    struct linkedlist dead_list;

    struct rbtree timeout_queue;
    uint64_t soonest_timeout;

    struct task *current;
};

static struct sched g_sched;
static struct task g_kernel;
static struct task g_idle;
alignas(PAGE_SIZE) static char g_idle_stack[PAGE_SIZE];

static void idle_task(uintptr_t);
static void irqmsg_timeout(struct irqmsg);

void sched_init(void) {
    g_sched.tid_next = 0;
    rbtree_init(&g_sched.tid_tree);
    slab_create_for(&g_sched.task_slab, struct task);
    linkedlist_init(&g_sched.ready_queue);
    linkedlist_init(&g_sched.dead_list);
    rbtree_init(&g_sched.timeout_queue);
    g_sched.soonest_timeout = UINT64_MAX;

    memset(&g_kernel, 0, sizeof(g_kernel));
    g_kernel.id = g_sched.tid_next++;
    g_kernel.refcount = 1;
    g_kernel.state = TASK_RUNNING;
    g_kernel.stack = NULL;
    g_sched.current = &g_kernel;

    memset(&g_idle, 0, sizeof(g_idle));
    g_idle.id = g_sched.tid_next++;
    g_idle.refcount = 1;
    g_idle.state = TASK_READY;
    g_idle.stack = NULL;
    context_init(&g_idle.ctx, (uintptr_t)idle_task, g_idle_stack, sizeof(g_idle_stack), 0);
    linkedlist_push_back(&g_sched.ready_queue, &g_idle.queue_link);

    irqmsg_register(IRQMSG_SCHED_TIMEOUT, irqmsg_timeout);
}

static void idle_task(uintptr_t) {
    while (1) {
        interrupts_disable();

        schedule();

        struct linkedlist_link *link = linkedlist_pop_front(&g_sched.dead_list);
        if (link) {
            struct task *task = container_of(link, struct task, queue_link);
            task_release(task);
        }

        interrupts_enable_and_wait();
    }
}

static void irqmsg_timeout(struct irqmsg) {
    irqlock_t irqlock = irqlock_acquire();

    struct rbtree_node *node = rbtree_first(&g_sched.timeout_queue);
    while (node) {
        struct task *task = container_of(node, struct task, timeout_node);
        if (task->wait_timeout > timer_get_tick()) {
            g_sched.soonest_timeout = task->wait_timeout;
            break;
        }

        node = rbtree_next(node);
        rbtree_remove(&g_sched.timeout_queue, &task->timeout_node);

        if (task->wait_for) {
            linkedlist_remove(&task->queue_link);
            task->wait_for = NULL;
        }

        task->state = TASK_READY;
        linkedlist_push_back(&g_sched.ready_queue, &task->queue_link);
    }

    irqlock_release(&irqlock);
}

void schedule(void) {
    irqlock_t irqlock = irqlock_acquire();

    if (g_sched.soonest_timeout <= timer_get_tick()) {
        if (irqmsg_push((struct irqmsg){ .type = IRQMSG_SCHED_TIMEOUT })) {
            g_sched.soonest_timeout = UINT64_MAX;
        }
    }

    struct linkedlist_link *link = linkedlist_pop_front(&g_sched.ready_queue);
    if (!link) {
        goto exit;
    }

    struct task *current = g_sched.current;
    struct task *next = container_of(link, struct task, queue_link);

    next->state = TASK_RUNNING;
    if (current->state == TASK_RUNNING) {
        current->state = TASK_READY;
        linkedlist_push_back(&g_sched.ready_queue, &current->queue_link);
    }

    g_sched.current = next;
    context_switch(&current->ctx, &next->ctx);

exit:
    irqlock_release(&irqlock);
}

struct task *task_create(void (*entry)(uintptr_t), uintptr_t arg) {
    irqlock_t irqlock = irqlock_acquire();

    struct task *task = slab_alloc(&g_sched.task_slab);
    if (!task) {
        irqlock_release(&irqlock);
        return NULL;
    }

    pfn_t stack_page = mm_alloc_page(0);
    if (stack_page == PFN_INVALID) {
        slab_free(&g_sched.task_slab, task);
        irqlock_release(&irqlock);
        return NULL;
    }

    assert(g_sched.tid_next < INT_MAX);
    task->id = g_sched.tid_next++;
    rbtree_insert_task(&g_sched.tid_tree, task);

    task->refcount = 1;
    task->state = TASK_READY;
    linkedlist_push_back(&g_sched.ready_queue, &task->queue_link);

    task->wait_for = NULL;

    task->stack = mm_pfn_to_ptr(stack_page);
    context_init(&task->ctx, (uintptr_t)entry, task->stack, PAGE_SIZE, arg);

    irqlock_release(&irqlock);
    return task;
}

static void task_free_stack(struct task *task) {
    if (task->stack) {
        pfn_t stack_page = mm_ptr_to_pfn(task->stack);
        mm_free_page(stack_page, 0);
        task->stack = NULL;
    }
}

static void task_free(struct task *task) {
    task_free_stack(task);
    rbtree_remove(&g_sched.tid_tree, &task->tid_node);
    slab_free(&g_sched.task_slab, task);
}

void task_terminate(struct task *task) {
    irqlock_t irqlock = irqlock_acquire();

    switch (task->state) {
        case TASK_DEAD:
            panic("cannot terminate dead task");
        case TASK_RUNNING:
            panic("cannot terminate current task");
        case TASK_READY:
            linkedlist_remove(&task->queue_link);
            break;
        case TASK_WAITING:
            if (task->has_timeout) {
                rbtree_remove(&g_sched.timeout_queue, &task->timeout_node);
            }
            if (task->wait_for) {
                linkedlist_remove(&task->queue_link);
                task->wait_for = NULL;
            }
            break;
    }

    task->state = TASK_DEAD;
    task_free_stack(task);

    irqlock_release(&irqlock);
}

[[noreturn]] void task_exit(void) {
    irqlock_t irqlock = irqlock_acquire();

    g_sched.current->state = TASK_DEAD;
    g_sched.current->refcount++;
    linkedlist_push_back(&g_sched.dead_list, &g_sched.current->queue_link);
    schedule();

    panic("unreachable!");
    (void)irqlock;
}

struct task *task_from_id(tid_t id) {
    irqlock_t irqlock = irqlock_acquire();

    struct rbtree_find_result result = rbtree_find_task(&g_sched.tid_tree, id);
    if (result.lower == NULL || result.lower != result.upper) {
        irqlock_release(&irqlock);
        return NULL;
    }

    struct task *task = container_of(result.lower, struct task, tid_node);
    assert(task->refcount < UINT_MAX);
    task->refcount++;

    irqlock_release(&irqlock);
    return task;
}

tid_t task_release(struct task *task) {
    irqlock_t irqlock = irqlock_acquire();

    assert(task->refcount > 0);
    task->refcount--;

    tid_t id = task->id;

    if (task->refcount == 0 && task->state == TASK_DEAD) {
        task_free(task);
        id = TID_INVALID;
    }

    irqlock_release(&irqlock);
    return id;
}

static uint64_t timeout_tick(uint32_t ms) {
    return timer_get_tick() + (uint64_t)ms * TIMER_HZ / 1000;
}

bool task_wait_for(struct waitable *obj, uint32_t ms) {
    irqlock_t irqlock = irqlock_acquire();

    if (obj && obj->triggered) {
        if (obj->reset) {
            obj->triggered = false;
        }
        irqlock_release(&irqlock);
        return true;
    }

    struct task *current = g_sched.current;
    current->state = TASK_WAITING;
    current->wait_for = obj;
    if (obj) {
        linkedlist_push_back(&obj->wait_queue, &current->queue_link);
    }

    if (ms == TIMEOUT_INFINITY) {
        current->has_timeout = false;
    } else {
        current->has_timeout = true;
        current->wait_timeout = timeout_tick(ms);
        rbtree_insert_timeout(&g_sched.timeout_queue, current);

        if (g_sched.soonest_timeout > current->wait_timeout) {
            g_sched.soonest_timeout = current->wait_timeout;
        }
    }

    schedule();

    bool timeout = current->has_timeout;
    current->has_timeout = false;

    irqlock_release(&irqlock);
    return !timeout;
}

void waitable_trigger(struct waitable *obj) {
    assert(obj);

    irqlock_t irqlock = irqlock_acquire();

    obj->triggered = true;
    while (1) {
        struct linkedlist_link *link = linkedlist_pop_front(&obj->wait_queue);
        if (!link) {
            break;
        }

        struct task *task = container_of(link, struct task, queue_link);

        task->state = TASK_READY;
        task->wait_for = NULL;
        if (task->has_timeout) {
            rbtree_remove(&g_sched.timeout_queue, &task->timeout_node);
            task->has_timeout = false;
        }

        linkedlist_push_back(&g_sched.ready_queue, &task->queue_link);

        if (obj->reset) {
            obj->triggered = false;
            break;
        }
    }

    irqlock_release(&irqlock);
}

void task_sleep(uint32_t ms) {
    task_wait_for(NULL, ms);
}
