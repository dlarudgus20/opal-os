#ifndef OPAL_PLATFORM_PC_X64_ASM_H
#define OPAL_PLATFORM_PC_X64_ASM_H

#include <stdint.h>

static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("out %1, %0" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("in %0, %1" : "=a"(value) : "Nd"(port));
    return value;
}

#endif
