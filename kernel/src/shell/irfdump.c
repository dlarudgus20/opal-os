#include <kc/string.h>

#include <opal/tty.h>
#include <opal/kargs.h>
#include <opal/shell/utils.h>
#include <opal/platform/mm/defines.h>
#include <opal/platform/boot/bootinfo.h>

int shell_cmd_irfdump(int, char **) {
    const struct bootinfo_module *module = kargs_get()->initramfs;
    if (!module) {
        tty0_puts("irfdump: initramfs is not set\n");
        return 1;
    }

    if (module->end <= module->begin) {
        tty0_puts("irfdump: initramfs range is invalid\n");
        return 1;
    }

    size_t total_len = module->end - module->begin;

    const unsigned char *ptr = (const unsigned char *)(DIRECT_MAP_START_VIRT + module->begin);
    tty0_printf("irfdump: [%#018"PRIphys", %#018"PRIphys") %s (%zu bytes)\n",
        module->begin, module->end, module->name, total_len);

    shell_hexdump(ptr, total_len);
    return 0;
}
