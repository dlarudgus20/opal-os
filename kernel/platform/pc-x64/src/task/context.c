#include <kc/kassert.h>

#include <opal/task/process.h>
#include <opal/task/task.h>
#include <opal/platform/asm.h>
#include <opal/platform/descriptors.h>
#include <opal/platform/task/context.h>
#include <opal/platform/mm/pagetable.h>

struct ctx_stack {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbx;
    uint64_t rbp;
    uint64_t rip;
    uint64_t dummy; // SysV ABI requirement; see below
};

// SysV ABI requires rsp % 16 == 8 at the start of a function
// including returning rip, sizeof(struct ctx_stack) must be a multiple of 16
static_assert(sizeof(struct ctx_stack) % 16 == 0);

// context.asm
void context_switch_asm(struct context *from, const struct context *to);
[[noreturn]] void enter_userland_asm(virt_addr_t entry, virt_addr_t stack_top, uint64_t cs, uint64_t ss);

static struct task *g_fpu_owner;

static void set_task_switched(void) {
    write_cr0(read_cr0() | CR0_TS);
}

static void clear_task_switched(void) {
    clear_cr0_ts();
}

void context_init(struct context *ctx, virt_addr_t entry, virt_addr_t stack, virt_size_t stack_size) {
    kassert(stack % PAGE_SIZE == 0);
    kassert(stack_size % PAGE_SIZE == 0);
    kassert(stack_size >= PAGE_SIZE);

    struct ctx_stack *rsp = (struct ctx_stack *)(stack + stack_size - sizeof(*rsp));
    rsp->rip = entry;
    ctx->rsp = (uint64_t)rsp;
    ctx->fpu_initialized = false;
}

void context_destroy(struct task *task) {
    if (g_fpu_owner == task) {
        g_fpu_owner = NULL;
        set_task_switched();
    }
}

void context_switch(struct task *from, const struct task *to) {
    if (from->process.ptr->pagetable != to->process.ptr->pagetable) {
        pagetable_apply(to->process.ptr->pagetable);
    }
    descriptors_set_kstack((uintptr_t)to->kstack + PAGE_SIZE);

    set_task_switched();
    context_switch_asm(&from->ctx, &to->ctx);
}

static void fpu_ctx_init(void) {
    fninit();
    const uint32_t mxcsr = 0x1f80;
    ldmxcsr(&mxcsr);
}

void fpu_init(void) {
    struct cpuid_info info = cpuid(1, 0);
    kassert((info.edx & CPUID_1_FXSR) != 0, "cpu does not support FXSAVE/FXRSTOR");
    kassert((info.edx & CPUID_1_SSE) != 0, "cpu does not support SSE");

    uint64_t cr0 = read_cr0();
    cr0 |= CR0_MP | CR0_NE;
    cr0 &= ~(CR0_EM | CR0_TS);
    write_cr0(cr0);

    uint64_t cr4 = read_cr4();
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    write_cr4(cr4);

    fpu_ctx_init();

    g_fpu_owner = NULL;
    set_task_switched();
}

void fpu_on_device_not_available(void) {
    clear_task_switched();

    struct task *current = task_current();
    if (g_fpu_owner == current) {
        return;
    }

    if (g_fpu_owner) {
        fxsave(g_fpu_owner->ctx.fpu_ctx);
    }

    if (current->ctx.fpu_initialized) {
        fxrstor(current->ctx.fpu_ctx);
    } else {
        fpu_ctx_init();
        current->ctx.fpu_initialized = true;
    }

    g_fpu_owner = current;
}

[[noreturn]] void enter_userland(virt_addr_t entry, virt_addr_t stack_top) {
    enter_userland_asm(entry, stack_top, USER_CODE_SEGMENT, USER_DATA_SEGMENT);
}
