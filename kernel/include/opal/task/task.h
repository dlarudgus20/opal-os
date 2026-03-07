#ifndef OPAL_TASK_TASK_H
#define OPAL_TASK_TASK_H

#include <stdint.h>
#include <stddef.h>

#include <collections/linkedlist.h>
#include <collections/rbtree.h>

#include <opal/platform/task/context.h>

#define TID_INVALID -1
#define TIMEOUT_INFINITY UINT32_MAX

typedef int tid_t;

enum task_state {
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING,
    TASK_DEAD,
};

struct waitable;

struct task {
    tid_t id;
    struct rbtree_node tid_node;

    unsigned refcount;

    enum task_state state;
    struct linkedlist_link queue_link;

    struct waitable *wait_for;

    bool has_timeout;
    struct rbtree_node timeout_node;
    uint64_t wait_timeout;

    struct context ctx;
    void *stack;
};

void sched_init(void);
void schedule(void);

[[nodiscard]] struct task *task_create(void (*entry)(uintptr_t), uintptr_t arg);
void task_terminate(struct task *task);

[[nodiscard]] struct task *task_from_id(tid_t id);
tid_t task_release(struct task *task);

[[noreturn]] void task_exit(void);
bool task_wait_for(struct waitable *obj, uint32_t ms);
void task_sleep(uint32_t ms);

#endif
