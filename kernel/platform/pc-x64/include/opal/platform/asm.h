#ifndef OPAL_PLATFORM_PC_X64_ASM_H
#define OPAL_PLATFORM_PC_X64_ASM_H

#include <stddef.h>
#include <stdint.h>

#include <kc/attributes.h>

#if __has_attribute(always_inline)
#define ALWAYS_INLINE static inline __attribute__((always_inline))
#else
#define ALWAYS_INLINE static inline
#endif

ALWAYS_INLINE void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("out %1, %0" : : "a"(value), "Nd"(port));
}

ALWAYS_INLINE uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("in %0, %1" : "=a"(value) : "Nd"(port));
    return value;
}

ALWAYS_INLINE void disable_interrupts(void) {
    __asm__ volatile ("cli" : : : "memory");
}

ALWAYS_INLINE void wait_for_interrupt(void) {
    __asm__ volatile ("hlt" : : : "memory");
}

ALWAYS_INLINE uint64_t read_cr2(void) {
    uint64_t value;
    __asm__ volatile ("mov %0, cr2" : "=r"(value) : : "memory");
    return value;
}

ALWAYS_INLINE uint64_t read_cr3(void) {
    uint64_t value;
    __asm__ volatile ("mov %0, cr3" : "=r"(value) : : "memory");
    return value;
}

ALWAYS_INLINE void write_cr3(uint64_t value) {
    __asm__ volatile ("mov cr3, %0" : : "r"(value) : "memory");
}

ALWAYS_INLINE void tlb_flush_for(uintptr_t va) {
    __asm__ volatile ( "invlpg [%0]" : : "r"(va) : "memory" );
}

ALWAYS_INLINE void load_gdt(void* gdt, size_t size) {
    struct {
        uint16_t size;
        void* base;
    } __attribute__((packed)) gdtr = { (uint16_t)(size - 1), gdt };
    __asm__ volatile ( "lgdt [%0]" : : "m"(gdtr) );
}

ALWAYS_INLINE void load_idt(void* idt, size_t size) {
    struct {
        uint16_t size;
        void* base;
    } __attribute__((packed)) idtr = { (uint16_t)(size - 1), idt };
    __asm__ volatile ( "lidt [%0]" : : "m"(idtr) );
}

ALWAYS_INLINE void load_tss(uint16_t selector) {
    __asm__ volatile ( "ltr %0" : : "r"(selector) );
}

#endif
