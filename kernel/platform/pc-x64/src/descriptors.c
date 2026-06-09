#include <kc/string.h>

#include <opal/attributes.h>
#include <opal/platform/descriptors.h>
#include <opal/platform/interrupt.h>
#include <opal/platform/asm.h>
#include <opal/platform/task/context.h>
#include <opal/platform/drivers/pic.h>

#define GDT_FLAG_ACCESSED   0x01
#define GDT_FLAG_RW         0x02
#define GDT_FLAG_DC         0x04
#define GDT_FLAG_CODE       0x08
#define GDT_FLAG_TSS_AVAIL  0x09
#define GDT_FLAG_TSS_BUSY   0x0b

#define GDT_FLAG_USER       0x10
#define GDT_FLAG_DPL3       0x60
#define GDT_FLAG_PRESENT    0x80

#define GDT_FLAG2           0xa
#define TSS_FLAG2           0

union PACKED gdt {
    struct PACKED {
        unsigned limit_low:16;
        unsigned base_low:24;
        unsigned flags1:8;
        unsigned limit_high:4;
        unsigned flags2:4;
        unsigned base_high:8;
    };
    uint64_t descriptor;
};

struct PACKED idt {
    unsigned offset_low:16;
    unsigned segment:16;
    unsigned ist:3;
    unsigned reserved1:5;
    unsigned type:4;
    unsigned reserved2:1;
    unsigned dpl:2;
    unsigned p:1;
    unsigned offset_mid:16;
    unsigned offset_high:32;
    unsigned reserved3:32;
};

static_assert(sizeof(union gdt) == 8);
static_assert(sizeof(struct idt) == 16);

static union gdt g_gdt[7];
static struct idt g_idt[256];
static struct tss g_tss;

static void init_gdt(union gdt *gdt, uint32_t base, uint32_t limit, uint8_t flags, uint8_t flags2) {
    gdt->limit_low = limit & 0xffff;
    gdt->base_low = base & 0xffffff;
    gdt->flags1 = flags;
    gdt->limit_high = (limit >> 16) & 0xf;
    gdt->flags2 = flags2;
    gdt->base_high = (base >> 24) & 0xff;
}

static void init_tss(union gdt *gdt, struct tss *tss) {
    init_gdt(gdt, (uint64_t)tss, sizeof(struct tss) - 1, GDT_FLAG_PRESENT | GDT_FLAG_TSS_AVAIL,
        TSS_FLAG2);
    gdt[1].descriptor = (uint64_t)tss >> 32;
}

static void init_idt(
    struct idt *idt, uint16_t segment, void (*handler)(), uint8_t dpl, uint8_t ist) {
    idt->offset_low = (uint64_t)handler & 0xffff;
    idt->segment = segment;
    idt->ist = ist;
    idt->reserved1 = 0;
    idt->type = 0xe;
    idt->reserved2 = 0;
    idt->dpl = dpl;
    idt->p = 1;
    idt->offset_mid = ((uint64_t)handler >> 16) & 0xffff;
    idt->offset_high = ((uint64_t)handler >> 32) & 0xffffffff;
    idt->reserved3 = 0;
}

void descriptors_init(void) {
    memset(g_gdt, 0, sizeof(g_gdt));
    memset(g_idt, 0, sizeof(g_idt));
    memset(&g_tss, 0, sizeof(g_tss));

    static alignas(16) char df_stack[0x2000];
    g_tss.ist[0] = (uint64_t)(df_stack + sizeof(df_stack));

    static_assert(KERNEL_CODE_SEGMENT == 1 * 8);
    static_assert(KERNEL_DATA_SEGMENT == 2 * 8);
    init_gdt(g_gdt + 1, 0, 0xffffffff, GDT_FLAG_PRESENT | GDT_FLAG_USER | GDT_FLAG_CODE, GDT_FLAG2);
    init_gdt(g_gdt + 2, 0, 0xffffffff, GDT_FLAG_PRESENT | GDT_FLAG_USER | GDT_FLAG_RW, GDT_FLAG2);
    init_gdt(g_gdt + 3, 0, 0xffffffff,
        GDT_FLAG_PRESENT | GDT_FLAG_USER | GDT_FLAG_DPL3 | GDT_FLAG_CODE, GDT_FLAG2);
    init_gdt(g_gdt + 4, 0, 0xffffffff,
        GDT_FLAG_PRESENT | GDT_FLAG_USER | GDT_FLAG_DPL3 | GDT_FLAG_RW, GDT_FLAG2);
    init_tss(g_gdt + 5, &g_tss);
    load_gdt(g_gdt, sizeof(g_gdt));
    load_tss(5 * 8);

    for (size_t i = 0; i < sizeof(g_idt) / sizeof(g_idt[0]); i++) {
        init_idt(g_idt + i, 0x08, isr_unknown, 0, 0);
    }

    init_idt(g_idt + 0, 0x08, isr_divide_by_zero, 0, 0);
    init_idt(g_idt + 1, 0x08, isr_debug, 0, 0);
    init_idt(g_idt + 2, 0x08, isr_nmi, 0, 0);
    init_idt(g_idt + 3, 0x08, isr_breakpoint, 0, 0);
    init_idt(g_idt + 4, 0x08, isr_overflow, 0, 0);
    init_idt(g_idt + 5, 0x08, isr_bound_range_exceeded, 0, 0);
    init_idt(g_idt + 6, 0x08, isr_invalid_opcode, 0, 0);
    init_idt(g_idt + 7, 0x08, isr_device_not_available, 0, 0);
    init_idt(g_idt + 8, 0x08, isr_double_fault, 0, 1);
    init_idt(g_idt + 10, 0x08, isr_invalid_tss, 0, 0);
    init_idt(g_idt + 11, 0x08, isr_segment_not_present, 0, 0);
    init_idt(g_idt + 12, 0x08, isr_stack_segment_fault, 0, 0);
    init_idt(g_idt + 13, 0x08, isr_general_protection_fault, 0, 0);
    init_idt(g_idt + 14, 0x08, isr_page_fault, 0, 0);
    init_idt(g_idt + 16, 0x08, isr_x87_floating_point, 0, 0);
    init_idt(g_idt + 17, 0x08, isr_alignment_check, 0, 0);
    init_idt(g_idt + 18, 0x08, isr_machine_check, 0, 0);
    init_idt(g_idt + 19, 0x08, isr_simd_floating_point, 0, 0);

    static_assert(PIC_INT_VECTOR == 32);
    init_idt(g_idt + 32, 0x08, isr_irq0, 0, 0);
    init_idt(g_idt + 33, 0x08, isr_irq1, 0, 0);
    init_idt(g_idt + 34, 0x08, isr_irq2, 0, 0);
    init_idt(g_idt + 35, 0x08, isr_irq3, 0, 0);
    init_idt(g_idt + 36, 0x08, isr_irq4, 0, 0);
    init_idt(g_idt + 37, 0x08, isr_irq5, 0, 0);
    init_idt(g_idt + 38, 0x08, isr_irq6, 0, 0);
    init_idt(g_idt + 39, 0x08, isr_irq7, 0, 0);
    init_idt(g_idt + 40, 0x08, isr_irq8, 0, 0);
    init_idt(g_idt + 41, 0x08, isr_irq9, 0, 0);
    init_idt(g_idt + 42, 0x08, isr_irq10, 0, 0);
    init_idt(g_idt + 43, 0x08, isr_irq11, 0, 0);
    init_idt(g_idt + 44, 0x08, isr_irq12, 0, 0);
    init_idt(g_idt + 45, 0x08, isr_irq13, 0, 0);
    init_idt(g_idt + 46, 0x08, isr_irq14, 0, 0);
    init_idt(g_idt + 47, 0x08, isr_irq15, 0, 0);
    init_idt(g_idt + 0x80, 0x08, isr_int80, 3, 0);

    load_idt(g_idt, sizeof(g_idt));
}

void descriptors_set_kstack(uintptr_t kstack_top) {
    g_tss.rsp[0] = kstack_top;
}
