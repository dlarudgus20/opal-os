#ifndef OPAL_PLATFORM_PC_X64_DEBUG_EXIT_H
#define OPAL_PLATFORM_PC_X64_DEBUG_EXIT_H

#ifdef OPAL_TEST

#define PLATFORM_HAS_DEBUG_EXIT

// Request a QEMU exit through isa-debug-exit. Without the device, this may return.
void platform_debug_exit(bool ok);

#endif

#endif
