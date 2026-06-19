#ifndef OPAL_PLATFORM_TASK_CONTEXT_H
#define OPAL_PLATFORM_TASK_CONTEXT_H

#include <opal/platform/mm/types.h>

struct task;
struct isr_stackframe;

struct context {
    uint64_t rsp;
    bool fpu_initialized;
    alignas(16) unsigned char fpu_ctx[512];
};

void context_init(
    struct context *ctx, virt_addr_t entry, virt_addr_t stack, virt_size_t stack_size);

void context_destroy(struct task *task);
void context_switch(struct task *from, const struct task *to);

void stackframe_set_return_value(struct isr_stackframe *frame, uintptr_t value);

void fpu_init(void);
void fpu_on_device_not_available(void);

[[noreturn]] void enter_userland(virt_addr_t entry, virt_addr_t stack_top);
[[noreturn]] void return_to_userland(const struct isr_stackframe *frame);

#endif
