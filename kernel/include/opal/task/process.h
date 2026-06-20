#ifndef OPAL_TASK_PROCESS_H
#define OPAL_TASK_PROCESS_H

#include <kc/kerrno.h>

#include <collections/linkedlist.h>
#include <collections/rbtree.h>

#include <opal/task/task.h>
#include <opal/task/filetable.h>
#include <opal/utils/vmtree.h>

#define PID_INVALID -1

typedef int pid_t;

struct pagetable;
struct isr_stackframe;

struct process {
    pid_t id;
    struct rbtree_node pid_node;

    unsigned refcount;

    struct linkedlist task_list;
    struct vmtree vmtree;
    struct pagetable *pagetable;

    struct filetable open_files;
};

void proc_tree_init(void);

void process_init(struct process *proc, struct pagetable *ptbl);
[[nodiscard]] procptr_t process_create(void);

struct process *process_current(void);
[[nodiscard]] procptr_t process_from_id(pid_t id);
[[nodiscard]] procptr_t process_retain(struct process *proc);
pid_t process_release(procptr_t proc);

fd_t process_open_file(struct process *proc, fd_t fd, struct file *file);
struct file *process_get_file(struct process *proc, fd_t fd);
bool process_close_file(struct process *proc, fd_t fd);
fd_t process_dup_fd(struct process *proc, fd_t oldfd, fd_t newfd);

[[nodiscard]] taskptr_t process_create_usertask(struct process *proc, virt_addr_t entry,
    virt_addr_t stack, virt_size_t stack_size, enum task_priority priority);

[[nodiscard]] kerrno_t process_load_elf(
    struct process *proc, void *elf, size_t size, taskptr_t *out);
[[nodiscard]] kerrno_t process_load_elf_file(
    struct file *file, procptr_t *proc_out, taskptr_t *task_out);

#endif
