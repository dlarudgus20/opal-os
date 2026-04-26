#include <limits.h>

#include <kc/inttypes.h>
#include <kc/assert.h>
#include <kc/string.h>

#include <opal/irq.h>
#include <opal/timer.h>
#include <opal/task/task.h>
#include <opal/task/process.h>
#include <opal/task/wait_list.h>
#include <opal/task/coroutine.h>
#include <opal/mm/mm.h>
#include <opal/mm/slab.h>
#include <opal/locks/irqlock.h>
#include <opal/platform/asm.h>
#include <opal/platform/mm/pagetable.h>

#define MAX_REFC INT_MAX
#define TID_END INT_MAX

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

    struct linkedlist ready_queue[TASK_PRIORITY_COUNT];
    struct linkedlist dead_list;

    struct rbtree timeout_queue;
    uint64_t soonest_timeout;

    struct task *current;
};

static struct process g_kproc;

static struct sched g_sched;
static struct task g_kernel;
static struct task g_idle;
alignas(PAGE_SIZE) static unsigned char g_idle_stack[PAGE_SIZE];

static void idle_task(uintptr_t);
static void irqmsg_timeout(struct irqmsg);

static void set_running(struct task *task) {
    task->state = TASK_RUNNING;
}

static void set_ready(struct task *task) {
    task->state = TASK_READY;
    linkedlist_push_back(&g_sched.ready_queue[task->priority], &task->queue_link);
}

static void reset_ready(struct task *task) {
    linkedlist_remove(&task->queue_link);
}

static void reset_wait_for(struct task *task) {
    if (!task->wait_for) {
        return;
    }

    linkedlist_remove(&task->queue_link);
    task->wait_for = NULL;
}

static void reset_timeout(struct task *task) {
    if (!task->has_timeout) {
        return;
    }

    rbtree_remove(&g_sched.timeout_queue, &task->timeout_node);
    task->has_timeout = false;
}

static void set_wait_for(struct task *task, struct wait_list *wl) {
    task->state = TASK_WAITING;
    task->wait_for = wl;
    if (wl) {
        linkedlist_push_back(&wl->queue, &task->queue_link);
    }
    task->has_timeout = false;
}

static void set_wait_timeout(struct task *task, struct wait_list *wl, uint64_t timeout) {
    set_wait_for(task, wl);

    task->has_timeout = true;
    task->wait_timeout = timeout;
    rbtree_insert_timeout(&g_sched.timeout_queue, task);
    if (g_sched.soonest_timeout > task->wait_timeout) {
        g_sched.soonest_timeout = task->wait_timeout;
    }
}

static void set_dead(struct task *task) {
    task->state = TASK_DEAD;
}

static void task_init(struct task *task, struct process *proc) {
    assert(g_sched.tid_next < TID_END);

    memset(task, 0, sizeof(*task));
    task->id = g_sched.tid_next++;
    task->process = process_retain(proc);
    task->refcount = 1;
    task->state = TASK_WAITING;
    task->priority = TASK_PRIORITY_NORMAL;
    task->wait_for = NULL;
    task->has_timeout = false;
    task->wait_timeout = 0;
    task->kstack = NULL;

    linkedlist_push_back(&proc->task_list, &task->proc_link);
    wait_list_init(&task->join_list);
    rbtree_insert_task(&g_sched.tid_tree, task);
}

void sched_init(void) {
    g_sched.tid_next = 0;
    rbtree_init(&g_sched.tid_tree);
    slab_create_for(&g_sched.task_slab, struct task);
    linkedlist_init(&g_sched.dead_list);
    rbtree_init(&g_sched.timeout_queue);
    g_sched.soonest_timeout = UINT64_MAX;

    for (size_t i = 0; i < TASK_PRIORITY_COUNT; i++) {
        linkedlist_init(&g_sched.ready_queue[i]);
    }

    proc_tree_init();
    process_init(&g_kproc, mm_kptbl_get());

    task_init(&g_kernel, &g_kproc);
    g_kernel.priority = TASK_PRIORITY_KERNEL;
    set_running(&g_kernel);
    g_sched.current = &g_kernel;

    task_init(&g_idle, &g_kproc);
    g_idle.priority = TASK_PRIORITY_IDLE;
    context_init(&g_idle.ctx, (virt_addr_t)idle_task, (virt_addr_t)g_idle_stack, sizeof(g_idle_stack));
    set_ready(&g_idle);

    fpu_init();
    coroutine_worker_init();

    irqmsg_register(IRQMSG_SCHED_TIMEOUT, irqmsg_timeout);
}

static bool drain_dead(void) {
    struct linkedlist_link *link = linkedlist_pop_front(&g_sched.dead_list);
    if (!link) {
        return false;
    }

    struct task *task = container_of(link, struct task, queue_link);
    task_release((taskptr_t){ .ptr = task });
    return true;
}

static void idle_task(uintptr_t) {
    while (1) {
        interrupts_disable();

        schedule();
        while (drain_dead()) {}

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

        reset_wait_for(task);
        set_ready(task);
    }

    irqlock_release(&irqlock);
}

static struct task *ready_queue_top(void) {
    for (size_t i = 0; i < TASK_PRIORITY_COUNT; i++) {
        struct linkedlist *queue = &g_sched.ready_queue[i];
        struct linkedlist_link *head = linkedlist_head(queue);
        if (linkedlist_is_nil(queue, head)) {
            continue;
        }
        return container_of(head, struct task, queue_link);
    }
    return NULL;
}

static void ready_queue_pop(struct task *task) {
    linkedlist_remove(&task->queue_link);
}

void schedule(void) {
    irqlock_t irqlock = irqlock_acquire();

    if (g_sched.soonest_timeout <= timer_get_tick()) {
        if (irqmsg_push((struct irqmsg){ .type = IRQMSG_SCHED_TIMEOUT })) {
            g_sched.soonest_timeout = UINT64_MAX;
        }
    }

    struct task *current = g_sched.current;
    struct task *next = ready_queue_top();
    if (!next) {
        goto exit;
    }

    if (current->state == TASK_RUNNING) {
        if (current->priority < next->priority) {
            goto exit;
        }

        set_ready(current);
    }

    ready_queue_pop(next);
    set_running(next);

    g_sched.current = next;
    context_switch(current, next);

exit:
    irqlock_release(&irqlock);
}

void sched_on_timer(void) {
    schedule();
}

static void ktask_entry(void) {
    uintptr_t *kstack = g_sched.current->kstack;
    ktask_entry_t entry = (ktask_entry_t)kstack[0];
    interrupts_enable();
    entry(kstack[1]);
    task_exit();
}

taskptr_t ktask_start(ktask_entry_t entry, uintptr_t arg, enum task_priority priority) {
    taskptr_t task = task_create(&g_kproc, ktask_entry, priority);
    if (!task.ptr) {
        return task;
    }
    uintptr_t *kstack = task.ptr->kstack;
    kstack[0] = (uintptr_t)entry;
    kstack[1] = arg;
    task_resume(task.ptr);
    return task;
}

taskptr_t task_create(struct process *proc, void (*entry)(void), enum task_priority priority) {
    assert(priority < TASK_PRIORITY_COUNT);

    void *kstack = mm_alloc_page_ptr(0);
    if (!kstack) {
        return (taskptr_t){ .ptr = NULL };
    }

    irqlock_t irqlock = irqlock_acquire();

    struct task *task = slab_alloc(&g_sched.task_slab);
    if (!task) {
        irqlock_release(&irqlock);
        mm_free_page_ptr(kstack, 0);
        return (taskptr_t){ .ptr = NULL };
    }

    task_init(task, proc);
    task->priority = priority;
    set_wait_for(task, NULL);

    task->kstack = kstack;
    context_init(&task->ctx, (virt_addr_t)entry, (virt_addr_t)kstack, PAGE_SIZE);

    irqlock_release(&irqlock);
    return (taskptr_t){ .ptr = task };
}

void task_suspend(struct task *task) {
    irqlock_t lock = irqlock_acquire();

    if (g_sched.current == task) {
        set_wait_for(task, NULL);
        schedule();
    } else {
        assert(task->state == TASK_READY);
        reset_ready(task);
        set_wait_for(task, NULL);
    }

    irqlock_release(&lock);
}

void task_resume(struct task *task) {
    irqlock_t lock = irqlock_acquire();

    assert(task->state == TASK_WAITING);
    assert(!task->wait_for);
    assert(!task->has_timeout);
    reset_wait_for(task);
    set_ready(task);

    irqlock_release(&lock);
}

static void task_free_stack(struct task *task) {
    if (!task->kstack) {
        return;
    }

    pfn_t stack_page = direct_ptr_to_pfn(task->kstack);
    mm_free_page(stack_page, 0);
    task->kstack = NULL;
}

static void task_free(struct task *task) {
    task_free_stack(task);
    linkedlist_remove(&task->proc_link);
    process_release(task->process);
    rbtree_remove(&g_sched.tid_tree, &task->tid_node);
    slab_free(&g_sched.task_slab, task);
}

void task_terminate(taskptr_t task) {
    irqlock_t irqlock = irqlock_acquire();

    switch (task.ptr->state) {
        case TASK_DEAD:
            panic("cannot terminate dead task");
        case TASK_RUNNING:
            panic("cannot terminate current task");
        case TASK_READY:
            reset_ready(task.ptr);
            break;
        case TASK_WAITING:
            reset_timeout(task.ptr);
            reset_wait_for(task.ptr);
            break;
    }

    set_dead(task.ptr);
    wait_list_wake_all(&task.ptr->join_list);

    context_destroy(task.ptr);
    task_free_stack(task.ptr);
    task_release(task);

    irqlock_release(&irqlock);
}

[[noreturn]] void task_exit(void) {
    irqlock_t irqlock = irqlock_acquire();

    set_dead(g_sched.current);
    wait_list_wake_all(&g_sched.current->join_list);

    context_destroy(g_sched.current);

    g_sched.current->refcount++;
    linkedlist_push_back(&g_sched.dead_list, &g_sched.current->queue_link);

    schedule();

    panic("unreachable!");
    (void)irqlock;
}

taskptr_t task_from_id(tid_t id) {
    irqlock_t irqlock = irqlock_acquire();

    struct rbtree_find_result result = rbtree_find_task(&g_sched.tid_tree, id);
    if (result.lower == NULL || result.lower != result.upper) {
        irqlock_release(&irqlock);
        return (taskptr_t){ .ptr = NULL };
    }

    struct task *task = container_of(result.lower, struct task, tid_node);
    assert(task->refcount < MAX_REFC);
    task->refcount++;

    irqlock_release(&irqlock);
    return (taskptr_t){ .ptr = task };
}

struct task *task_current(void) {
    return g_sched.current;
}

taskptr_t task_retain(struct task *task) {
    irqlock_t irqlock = irqlock_acquire();
    assert(task->refcount < MAX_REFC);
    task->refcount++;
    irqlock_release(&irqlock);
    return (taskptr_t){ .ptr = task };
}

tid_t task_release(taskptr_t task) {
    irqlock_t irqlock = irqlock_acquire();

    assert(task.ptr->refcount > 0);
    task.ptr->refcount--;

    tid_t id = task.ptr->id;

    if (task.ptr->refcount == 0 && task.ptr->state == TASK_DEAD) {
        task_free(task.ptr);
        id = TID_INVALID;
    }

    irqlock_release(&irqlock);
    return id;
}

bool wait_list_wake_one(struct wait_list *wl) {
    assert(wl);

    irqlock_t irqlock = irqlock_acquire();
    bool waken = true;

    struct linkedlist_link *link = linkedlist_pop_front(&wl->queue);
    if (!link) {
        waken = false;
        goto exit;
    }

    struct task *task = container_of(link, struct task, queue_link);

    task->wait_for = NULL;
    if (task->has_timeout) {
        reset_timeout(task);
    }
    set_ready(task);

exit:
    irqlock_release(&irqlock);
    return waken;
}

void wait_list_wake_all(struct wait_list *wl) {
    assert(wl);

    irqlock_t irqlock = irqlock_acquire();

    while (!linkedlist_is_empty(&wl->queue)) {
        wait_list_wake_one(wl);
    }

    irqlock_release(&irqlock);
}

bool task_wait(struct wait_list *wl, uint64_t timeout) {
    if (timeout <= timer_get_tick()) {
        return false;
    }

    irqlock_t irqlock = irqlock_acquire();

    if (timeout == TIMEOUT_INFINITY) {
        set_wait_for(g_sched.current, wl);
    } else {
        set_wait_timeout(g_sched.current, wl, timeout);
    }
    schedule();

    bool is_timeout = g_sched.current->has_timeout;
    g_sched.current->has_timeout = false;

    irqlock_release(&irqlock);
    return !is_timeout;
}

bool task_join(struct task *task, uint64_t timeout) {
    assert(task);
    assert(task != g_sched.current, "cannot join self");

    irqlock_t irqlock = irqlock_acquire();
    bool ok = true;

    if (task->state == TASK_DEAD) {
        goto exit;
    }

    if (timeout <= timer_get_tick()) {
        ok = false;
        goto exit;
    }

    ok = task_wait(&task->join_list, timeout);

exit:
    irqlock_release(&irqlock);
    return ok;
}
