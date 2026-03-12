#ifndef OPAL_TASK_TASK_H
#define OPAL_TASK_TASK_H

#include <stdint.h>
#include <stddef.h>

#include <collections/linkedlist.h>
#include <collections/rbtree.h>

#include <opal/task/wait_list.h>
#include <opal/platform/task/context.h>

#define TID_INVALID -1
#define TIMEOUT_INFINITY UINT64_MAX

typedef int tid_t;

enum task_state {
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING,
    TASK_DEAD,
};

struct task {
    tid_t id;
    struct rbtree_node tid_node;

    unsigned refcount;

    enum task_state state;
    struct linkedlist_link queue_link;

    struct wait_list join_list;

    struct wait_list *wait_for;
    bool has_timeout;
    struct rbtree_node timeout_node;
    uint64_t wait_timeout;

    struct context ctx;
    void *stack;
};

typedef struct taskptr {
    struct task *ptr;
} taskptr_t;

void sched_init(void);
void schedule(void);

[[nodiscard]] taskptr_t task_create(void (*entry)(uintptr_t), uintptr_t arg);
void task_terminate(taskptr_t task);

[[nodiscard]] taskptr_t task_from_id(tid_t id);
[[nodiscard]] struct task *task_current(void);
[[nodiscard]] taskptr_t task_addref(struct task *task);
tid_t task_release(taskptr_t task);

void task_exit(void);
bool task_wait(struct wait_list *wl, uint64_t timeout);
bool task_join(struct task *task, uint64_t timeout);

#endif
