#ifndef OPAL_PLATFORM_TASK_CONTEXT_H
#define OPAL_PLATFORM_TASK_CONTEXT_H

#include <stddef.h>

#include <opal/platform/interrupt.h>

struct context {
    ISR_STACKFRAME_PUSH
    ISR_STACKFRAME_IRETQ
};

#define PRI_CONTEXT         PRI_ISR_STACKFRAME
#define ARG_CONTEXT(ctx)    ARG_ISR_STACKFRAME(ctx)

void context_init(struct context *ctx, uintptr_t entry, void *stack, size_t stack_size, uintptr_t arg);

// context.asm
void context_switch(struct context *from, const struct context *to);

#endif
