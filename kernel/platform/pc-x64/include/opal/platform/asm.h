#ifndef OPAL_PLATFORM_PC_X64_ASM_H
#define OPAL_PLATFORM_PC_X64_ASM_H

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

#endif
