#include <kc/assert.h>

#include <opal/platform/asm.h>
#include <opal/platform/descriptors.h>
#include <opal/platform/task/context.h>
#include <opal/platform/mm/defines.h>
#include <opal/task/task.h>

static struct fpu_context g_fpu_init_ctx;
static struct task *g_fpu_owner;

static void set_task_switched(void) {
    write_cr0(read_cr0() | CR0_TS);
}

static void clear_task_switched(void) {
    clear_cr0_ts();
}

void context_init(struct context *ctx, uintptr_t entry, void *stack, size_t stack_size, uintptr_t arg) {
    assert((uintptr_t)stack % PAGE_SIZE == 0);
    assert(stack_size % PAGE_SIZE == 0);
    assert(stack_size >= PAGE_SIZE);

    ctx->rip = entry;
    ctx->cs = KERNEL_CODE_SEGMENT;
    // interrupt on
    ctx->rflags = rflags_get() | RFLAGS_INTR;
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

void fpu_init(void) {
    struct cpuid_info info = cpuid(1, 0);
    assert((info.edx & CPUID_1_FXSR) != 0, "cpu does not support FXSAVE/FXRSTOR");
    assert((info.edx & CPUID_1_SSE) != 0, "cpu does not support SSE");

    uint64_t cr0 = read_cr0();
    cr0 |= CR0_MP | CR0_NE;
    cr0 &= ~(CR0_EM | CR0_TS);
    write_cr0(cr0);

    uint64_t cr4 = read_cr4();
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    write_cr4(cr4);

    fninit();
    const uint32_t mxcsr = 0x1f80;
    ldmxcsr(&mxcsr);
    fxsave(&g_fpu_init_ctx);

    g_fpu_owner = NULL;
    set_task_switched();
}

void fpu_on_task_switch(struct task *from, struct task *to) {
    if (from != to) {
        set_task_switched();
    }
}

void fpu_on_task_exit(struct task *task) {
    if (g_fpu_owner == task) {
        g_fpu_owner = NULL;
        set_task_switched();
    }
}

void fpu_on_device_not_available(void) {
    clear_task_switched();

    struct task *current = task_current();
    if (g_fpu_owner == current) {
        return;
    }

    if (g_fpu_owner) {
        fxsave(&g_fpu_owner->fpu_ctx);
        g_fpu_owner->fpu_initialized = true;
    }

    if (current->fpu_initialized) {
        fxrstor(&current->fpu_ctx);
    } else {
        fxrstor(&g_fpu_init_ctx);
        current->fpu_initialized = true;
    }

    g_fpu_owner = current;
}
