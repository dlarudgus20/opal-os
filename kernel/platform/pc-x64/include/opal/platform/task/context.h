#ifndef OPAL_PLATFORM_TASK_CONTEXT_H
#define OPAL_PLATFORM_TASK_CONTEXT_H

#include <stddef.h>

#include <opal/attributes.h>
#include <opal/platform/interrupt.h>

struct task;

struct context {
    ISR_STACKFRAME_PUSH
    ISR_STACKFRAME_IRETQ
};

struct ALIGNED(16) fpu_context {
    char data[512];
};

#define PRI_CONTEXT         PRI_ISR_STACKFRAME
#define ARG_CONTEXT(ctx)    ARG_ISR_STACKFRAME(ctx)

void context_init(struct context *ctx, uintptr_t entry, void *stack, size_t stack_size, uintptr_t arg);
void fpu_init(void);
void fpu_on_task_switch(struct task *from, struct task *to);
void fpu_on_task_exit(struct task *task);
void fpu_on_device_not_available(void);

// context.asm
void context_switch(struct context *from, const struct context *to);

#endif
