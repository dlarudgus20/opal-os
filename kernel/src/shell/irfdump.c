#include <opal/tty.h>
#include <opal/kargs.h>
#include <opal/platform/mm/defines.h>
#include <opal/platform/boot/bootinfo.h>

static bool is_ascii_printable(unsigned char c) {
    return c >= 0x20 && c <= 0x7e;
}

int shell_cmd_irfdump(void) {
    const struct bootinfo_module *module = kargs_get()->initramfs;
    if (!module) {
        tty0_puts("irfdump: initramfs is not set\n");
        return 1;
    }

    if (module->end <= module->begin) {
        tty0_puts("irfdump: initramfs range is invalid\n");
        return 1;
    }

    size_t len = module->end - module->begin;
    if (len > 256) {
        len = 256;
    }

    const unsigned char *ptr = (const unsigned char *)(DIRECT_MAP_START_VIRT + module->begin);
    tty0_printf("irfdump: [%#018"PRIphys", %#018"PRIphys") %s (%zu bytes)\n",
        module->begin, module->end, module->name, len);

    for (size_t off = 0; off < len; off += 16) {
        tty0_printf("%08zx: ", off);

        for (size_t i = 0; i < 16; i++) {
            if (off + i < len) {
                tty0_printf("%02x ", ptr[off + i]);
            } else {
                tty0_puts("   ");
            }
        }

        tty0_puts(" |");
        for (size_t i = 0; i < 16 && off + i < len; i++) {
            unsigned char c = ptr[off + i];
            tty0_printf("%c", is_ascii_printable(c) ? c : '.');
        }
        tty0_puts("|\n");
    }
    return 1;
}
