#include <stddef.h>

#include <kc/string.h>

#include <opal/klog.h>
#include <opal/kargs.h>
#include <opal/fs/cpio.h>
#include <opal/fs/vfs.h>
#include <opal/mm/pfn.h>
#include <opal/platform/boot/bootinfo.h>

static struct kargs g_kargs;

static bool is_space(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

static bool key_is(const char *key, size_t key_len, const char *lit) {
    const size_t lit_len = strlen(lit);
    if (key_len != lit_len) {
        return false;
    }
    return memcmp(key, lit, lit_len) == 0;
}

static void opt_initramfs(const char *name, size_t len) {
    const struct bootinfo_module_list *modules = bootinfo_get_modules();
    for (uint32_t i = 0; i < modules->len; i++) {
        const struct bootinfo_module *module = &modules->modules[i];
        const size_t module_name_len = strnlen_s(module->name, MAX_BOOT_MODULE_NAME);
        if (module_name_len != len) {
            continue;
        }
        if (memcmp(module->name, name, len) != 0) {
            continue;
        }

        if (g_kargs.initramfs) {
            kwarn("kargs: duplicate option 'initramfs'; overriding previous value");
        }
        g_kargs.initramfs = module;
        return;
    }

    kwarn("kargs: initramfs module not found: %.*s", (int)len, name);
}

static void dispatch_option(const char *key, size_t key_len, const char *value, size_t value_len) {
    if (key_is(key, key_len, "initramfs")) {
        opt_initramfs(value, value_len);
        return;
    }

    kwarn("kargs: unknown option ignored: %.*s", (int)key_len, key);
}

static void parse(const char *args) {
    char decoded_value[MAX_BOOT_CMDLINE];

    const char *p = args;
    while (*p) {
        while (is_space(*p)) {
            p++;
        }
        if (*p == '\0') {
            break;
        }

        const char *key = p;
        while (*p && !is_space(*p) && *p != '=') {
            p++;
        }
        const size_t key_len = (size_t)(p - key);

        if (key_len == 0 || *p != '=') {
            kwarn("kargs: malformed token near: %s", key);
            while (*p && !is_space(*p)) {
                p++;
            }
            continue;
        }
        p++;

        const char *value = NULL;
        size_t value_len = 0;

        if (*p == '"') {
            p++;
            bool quote_closed = false;

            while (*p) {
                char out;
                if (*p == '"') {
                    quote_closed = true;
                    p++;
                    break;
                }

                if (*p == '\\') {
                    p++;
                    if (*p == '"') {
                        out = '"';
                        p++;
                    } else if (*p == '\\') {
                        out = '\\';
                        p++;
                    } else if (*p == '\0') {
                        out = '\\';
                    } else {
                        if (value_len + 1 >= sizeof(decoded_value)) {
                            kwarn("kargs: quoted value too long");
                            while (*p && !is_space(*p)) {
                                p++;
                            }
                            goto next_token;
                        }
                        decoded_value[value_len++] = '\\';
                        out = *p;
                        p++;
                    }
                } else {
                    out = *p;
                    p++;
                }

                if (value_len + 1 >= sizeof(decoded_value)) {
                    kwarn("kargs: quoted value too long");
                    while (*p && !is_space(*p)) {
                        p++;
                    }
                    goto next_token;
                }
                decoded_value[value_len++] = out;
            }

            if (!quote_closed) {
                kwarn("kargs: unterminated quoted value for option: %.*s", (int)key_len, key);
                break;
            }

            if (*p && !is_space(*p)) {
                kwarn("kargs: malformed token after quoted value");
                while (*p && !is_space(*p)) {
                    p++;
                }
                continue;
            }

            decoded_value[value_len] = '\0';
            value = decoded_value;
        } else {
            value = p;
            while (*p && !is_space(*p)) {
                p++;
            }
            value_len = (size_t)(p - value);
        }

        dispatch_option(key, key_len, value, value_len);
next_token:
        continue;
    }
}

void kargs_init(void) {
    memset(&g_kargs, 0, sizeof(g_kargs));
    parse(bootinfo_get_cmdline());

    kargs_print_log();
}

const struct kargs *kargs_get(void) {
    return &g_kargs;
}

static void postboot_initramfs(void) {
    const struct bootinfo_module *module = g_kargs.initramfs;
    if (!module) {
        return;
    }
    if (module->end <= module->begin) {
        kwarn("kargs: invalid initramfs range [%#018"PRIphys", %#018"PRIphys")",
            module->begin, module->end);
        return;
    }

    void *cpio = phys_to_direct_ptr(module->begin);
    size_t len = module->end - module->begin;

    struct superblock *sb = NULL;
    kerrno_t result = cpio_mount(cpio, len, &sb);
    if (result != OPAL_OK) {
        kwarn("kargs: failed to mount initramfs image: %s (%d)", kerrno_str(result), result);
        return;
    }

    struct path_entry *mounted = NULL;
    result = vfs_mount_path(NULL, "/", sb, &mounted);
    if (result != OPAL_OK) {
        kwarn("kargs: failed to mount initramfs on /: %s (%d)", kerrno_str(result), result);
        sb->ops->umount(sb);
        return;
    }

    path_entry_release(mounted);
    kinfo("kargs: mounted initramfs on /");
}

void kargs_postboot(void) {
    postboot_initramfs();
}

void kargs_print_log(void) {
    kinfo("boot args: %s", bootinfo_get_cmdline());
    if (g_kargs.initramfs) {
        kinfo(" initramfs: [%#018"PRIphys", %#018"PRIphys") %s",
            g_kargs.initramfs->begin, g_kargs.initramfs->end, g_kargs.initramfs->name);
    } else {
        kinfo(" initramfs: (null)");
    }
}
