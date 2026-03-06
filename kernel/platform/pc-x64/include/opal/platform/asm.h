#ifndef OPAL_PLATFORM_PC_X64_ASM_H
#define OPAL_PLATFORM_PC_X64_ASM_H

#include <stddef.h>
#include <stdint.h>

#include <opal/attributes.h>

#if __has_attribute(always_inline)
#define ALWAYS_INLINE [[gnu::always_inline]] static inline
#else
#define ALWAYS_INLINE static inline
#endif

#define RFLAGS_CARRY    (1 << 0)
#define RFLAGS_PARITY   (1 << 1)
#define RFLAGS_AUXCARRY (1 << 4)
#define RFLAGS_ZERO     (1 << 6)
#define RFLAGS_SIGN     (1 << 7)
#define RFLAGS_TRAP     (1 << 8)
#define RFLAGS_INTR     (1 << 9)
#define RFLAGS_DIR      (1 << 10)
#define RFLAGS_OVERFLOW (1 << 11)

ALWAYS_INLINE void out8(uint16_t port, uint8_t value) {
    __asm__ volatile ("out %1, %0" : : "a"(value), "Nd"(port));
}

ALWAYS_INLINE uint8_t in8(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("in %0, %1" : "=a"(value) : "Nd"(port));
    return value;
}

ALWAYS_INLINE uint64_t rflags_get(void) {
    uint64_t flags;
    __asm__ volatile ( "pushfq; pop %0" : "=r"(flags) );
    return flags;
}

ALWAYS_INLINE void rflags_set(uint64_t flags) {
    __asm__ volatile ( "push %0; popfq" : : "r"(flags) );
}

ALWAYS_INLINE bool interrupt_is_enabled(void) {
    return (rflags_get() & RFLAGS_INTR) != 0;
}

// cli / sti instruction is used for critical sections in many cases
// thus "memory" clobber is needed

ALWAYS_INLINE void interrupts_disable(void) {
    __asm__ volatile ("cli" : : : "memory");
}

ALWAYS_INLINE void interrupts_enable(void) {
    __asm__ volatile ("sti" : : : "memory");
}

ALWAYS_INLINE void wait_for_interrupt(void) {
    __asm__ volatile ("hlt");
}

ALWAYS_INLINE void interrupts_enable_and_wait(void) {
    __asm__ volatile ("sti; hlt" : : : "memory");
}

ALWAYS_INLINE uint64_t read_cr2(void) {
    uint64_t value;
    __asm__ volatile ("mov %0, cr2" : "=r"(value));
    return value;
}

ALWAYS_INLINE uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %0, cr3" : "=r"(value));
    return value;
}

ALWAYS_INLINE void write_cr3(uint64_t value) {
    __asm__ volatile ("mov cr3, %0" : : "r"(value) : "memory");
}

ALWAYS_INLINE void tlb_flush_for(uintptr_t va) {
    __asm__ volatile ( "invlpg [%0]" : : "r"(va) : "memory" );
}

ALWAYS_INLINE void load_gdt(void* gdt, size_t size) {
    struct PACKED {
        uint16_t size;
        void* base;
    } gdtr = { (uint16_t)(size - 1), gdt };
    __asm__ volatile ( "lgdt [%0]" : : "m"(gdtr) );
}

ALWAYS_INLINE void load_idt(void* idt, size_t size) {
    struct PACKED {
        uint16_t size;
        void* base;
    } idtr = { (uint16_t)(size - 1), idt };
    __asm__ volatile ( "lidt [%0]" : : "m"(idtr) );
}

ALWAYS_INLINE void load_tss(uint16_t selector) {
    __asm__ volatile ( "ltr %0" : : "r"(selector) );
}

#endif
