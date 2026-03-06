#include <kc/assert.h>

#include <opal/platform/asm.h>
#include <opal/platform/descriptors.h>
#include <opal/platform/task/context.h>
#include <opal/platform/mm/defines.h>

void context_init(struct context *ctx, uintptr_t entry, void *stack, size_t stack_size, uintptr_t arg) {
    assert((uintptr_t)stack % PAGE_SIZE == 0);
    assert(stack_size % PAGE_SIZE == 0);
    assert(stack_size >= PAGE_SIZE);

    ctx->rip = entry;
    ctx->cs = KERNEL_CODE_SEGMENT;
    ctx->rflags = rflags_get();
    // SysV ABI requires rsp % 16 == 8
    ctx->rsp = (uintptr_t)stack + stack_size - sizeof(uintptr_t);
    ctx->rbp = ctx->rsp;
    ctx->ss = KERNEL_DATA_SEGMENT;
    ctx->ds = KERNEL_DATA_SEGMENT;
    ctx->es = KERNEL_DATA_SEGMENT;
    ctx->fs = KERNEL_DATA_SEGMENT;
    ctx->gs = KERNEL_DATA_SEGMENT;
    ctx->rdi = arg;
}
