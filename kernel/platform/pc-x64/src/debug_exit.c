#include <opal/platform/asm.h>
#include <opal/platform/debug_exit.h>

#define QEMU_DEBUG_EXIT_PORT 0x501

void platform_debug_exit(bool ok) {
    out8(QEMU_DEBUG_EXIT_PORT, ok ? 0x10 : 0x11);
}
