#ifndef OPAL_PLATFORM_TASK_CONTEXT_H
#define OPAL_PLATFORM_TASK_CONTEXT_H

#include <opal/platform/mm/types.h>

struct task;

struct context {
    uint64_t rsp;
    bool fpu_initialized;
    alignas(16) unsigned char fpu_ctx[512];
};

void context_init(struct context *ctx, virt_addr_t entry, virt_addr_t stack, virt_size_t stack_size);
void context_destroy(struct task *task);
void context_switch(struct task *from, const struct task *to);

void fpu_init(void);
void fpu_on_device_not_available(void);

[[noreturn]] void enter_userland(virt_addr_t entry, virt_addr_t stack_top);

#endif
