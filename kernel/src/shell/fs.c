#include <limits.h>

#include <kc/string.h>
#include <kc/stdlib.h>

#include <opal/tty.h>
#include <opal/kargs.h>
#include <opal/fs/vfs.h>
#include <opal/fs/fat.h>
#include <opal/fs/cpio.h>
#include <opal/fs/block_device.h>
#include <opal/shell/shell_cmd.h>
#include <opal/mm/pfn.h>
#include <opal/platform/boot/bootinfo.h>

static bool parse_bdev_arg(const char *cmd, const char *arg, struct block_device **dev_out) {
    unsigned long bdev_ul = 0;
    if (kstrtoul_exact(arg, 10, ULONG_MAX, &bdev_ul) != OPAL_OK) {
        tty0_printf("%s: invalid bdev '%s'\n", cmd, arg);
        return false;
    }

    kerrno_t result = bdev_list_get((size_t)bdev_ul, dev_out);
    if (result != OPAL_OK) {
        tty0_printf("%s: invalid bdev '%s'\n", cmd, arg);
        return false;
    }

    return true;
}

static kerrno_t mount_fat(struct block_device *dev, const char *mount_path) {
    struct superblock *sb = NULL;
    kerrno_t result = fat_mount(dev, &sb);
    if (result != OPAL_OK) {
        block_device_release(dev);
        return result;
    }

    struct path_entry *mounted;
    result = vfs_mount_path(NULL, mount_path, sb, &mounted);
    if (result != OPAL_OK) {
        sb->ops->umount(sb);
        return result;
    }
    path_entry_release(mounted);
    return OPAL_OK;
}

static kerrno_t mount_cpio(const char *source, const char *mount_path) {
    if (strcmp(source, "initramfs") != 0) {
        return OPAL_EINVAL;
    }

    const struct bootinfo_module *module = kargs_get()->initramfs;
    if (!module || module->end <= module->begin) {
        return OPAL_ENOENT;
    }

    void *cpio = phys_to_direct_ptr(module->begin);
    size_t len = module->end - module->begin;

    struct superblock *sb = NULL;
    kerrno_t result = cpio_mount(cpio, len, &sb);
    if (result != OPAL_OK) {
        return result;
    }

    struct path_entry *mounted;
    result = vfs_mount_path(NULL, mount_path, sb, &mounted);
    if (result != OPAL_OK) {
        sb->ops->umount(sb);
        return result;
    }
    path_entry_release(mounted);
    return OPAL_OK;
}

static kerrno_t mkfs_fat(struct block_device *dev, bool auto_mount, const char *mount_path) {
    struct superblock *sb = NULL;
    kerrno_t result = fat_format(dev, &sb);
    if (result != OPAL_OK) {
        block_device_release(dev);
        return result;
    }

    if (auto_mount) {
        struct path_entry *mounted;
        result = vfs_mount_path(NULL, mount_path, sb, &mounted);
        if (result != OPAL_OK) {
            sb->ops->umount(sb);
            return result;
        }
        path_entry_release(mounted);
    } else {
        sb->ops->umount(sb);
    }

    return OPAL_OK;
}

static int print_cat_error(const char *op, kerrno_t result) {
    tty0_printf("cat: %s: %s (%d)\n", op, kerrno_str(result), result);
    return 1;
}

static int print_ls_error(const char *op, kerrno_t result) {
    tty0_printf("ls: %s: %s (%d)\n", op, kerrno_str(result), result);
    return 1;
}

static int print_mkdir_error(const char *op, kerrno_t result) {
    tty0_printf("mkdir: %s: %s (%d)\n", op, kerrno_str(result), result);
    return 1;
}

int shell_cmd_cat(int argc, char **argv) {
    bool write_mode = false;
    const char *path = NULL;

    if (argc == 2) {
        path = argv[1];
    } else if (argc == 3 && strcmp(argv[1], ">") == 0) {
        write_mode = true;
        path = argv[2];
    } else {
        tty0_puts("usage: cat /path\n");
        tty0_puts("   or: cat > /path\n");
        return 1;
    }

    struct file *file = NULL;
    kerrno_t result;
    if (write_mode) {
        result = vfs_create_path(NULL, path, 0, true, &file);
        if (result != OPAL_OK) {
            return print_cat_error("lookup/create", result);
        }
    } else {
        result = vfs_open_path(NULL, path, &file);
        if (result != OPAL_OK) {
            return print_cat_error("open", result);
        }
    }

    int rc = 0;

    if (!write_mode) {
        char buffer[256];
        fs_size_t pos = 0;
        while (1) {
            fs_ssize_t n = file->ops->read(file, pos, buffer, sizeof(buffer));
            if (n < 0) {
                rc = print_cat_error("read", n);
                break;
            }
            if (n == 0) {
                break;
            }

            tty0_puts_len(buffer, (size_t)n);
            pos += (fs_size_t)n;
        }
    } else {
        if (!file->ops->truncate) {
            rc = print_cat_error("truncate", OPAL_ENOTSUPP);
        } else {
            result = file->ops->truncate(file, 0);
            if (result != OPAL_OK) {
                rc = print_cat_error("truncate", result);
            }
        }

        if (rc == 0) {
            char line[128];
            while (1) {
                tty0_getline(line, sizeof(line));
                if (strcmp(line, "EOF") == 0) {
                    break;
                }

                size_t line_len = strlen(line);
                fs_ssize_t n = file->ops->write(file, 0, line, line_len, true);
                if (n < 0) {
                    rc = print_cat_error("write", n);
                    break;
                }
                if ((size_t)n != line_len) {
                    rc = print_cat_error("write", OPAL_EIO);
                    break;
                }

                n = file->ops->write(file, 0, "\n", 1, true);
                if (n < 0) {
                    rc = print_cat_error("write", n);
                    break;
                }
                if (n != 1) {
                    rc = print_cat_error("write", OPAL_EIO);
                    break;
                }
            }
        }
    }

    file_release(file);
    return rc;
}

int shell_cmd_ls(int argc, char **argv) {
    const char *path = "/";
    if (argc == 2) {
        path = argv[1];
    } else if (argc != 1) {
        tty0_puts("usage: ls [path]\n");
        return 1;
    }

    int ec = 0;
    struct path_entry *pe = NULL;
    kerrno_t result = vfs_lookup_path(NULL, path, &pe, NULL);
    if (result != OPAL_OK) {
        ec = print_ls_error("lookup_path", result);
        goto end;
    }
    if (!pe || !pe->inode) {
        ec = print_ls_error("lookup_path", OPAL_ENOENT);
        goto end;
    }
    if (!(pe->inode->flags & FS_INODE_DIR)) {
        ec = print_ls_error("lookup_path", OPAL_ENOTDIR);
        goto end;
    }
    if (!pe->inode->ops || !pe->inode->ops->lookup) {
        ec = print_ls_error("lookup", OPAL_ENOTSUPP);
        goto end;
    }

    result = pe->inode->ops->lookup(pe->inode, pe);
    if (result != OPAL_OK) {
        ec = print_ls_error("lookup", result);
        goto end;
    }

    tty0_printf("%s:\n", path);
    linkedlist_foreach(ptr, &pe->children) {
        struct path_entry *child = container_of(ptr, struct path_entry, link);
        if (!child->inode) {
            continue;
        }
        bool is_dir = child->inode && (child->inode->flags & FS_INODE_DIR);
        tty0_printf("%s%s\n", hstrget(&child->name), is_dir ? "/" : "");
    }

end:
    if (pe) {
        path_entry_release(pe);
    }
    return ec;
}

int shell_cmd_mkdir(int argc, char **argv) {
    if (argc != 2) {
        tty0_puts("usage: mkdir /path\n");
        return 1;
    }

    struct file *file = NULL;
    kerrno_t result = vfs_create_path(NULL, argv[1], FS_INODE_DIR, false, &file);
    if (result != OPAL_OK) {
        return print_mkdir_error("create", result);
    }

    file_release(file);
    return 0;
}

int shell_cmd_mount(int argc, char **argv) {
    if (argc != 4) {
        tty0_puts("usage: mount [fstype] [source] [mount-path]\n");
        return 1;
    }

    kerrno_t result = OPAL_EUNKNOWN;
    if (strcmp(argv[1], "fat") == 0) {
        struct block_device *dev = NULL;
        if (!parse_bdev_arg("mount", argv[2], &dev)) {
            return 1;
        }
        result = mount_fat(dev, argv[3]);
    } else if (strcmp(argv[1], "cpio") == 0) {
        result = mount_cpio(argv[2], argv[3]);
    } else {
        tty0_printf("mount: unsupported fstype '%s'\n", argv[1]);
        return 1;
    }

    if (result != OPAL_OK) {
        tty0_printf("mount: error %s (%d)\n", kerrno_str(result), result);
        return 1;
    }

    tty0_puts("mount: done\n");
    return 0;
}

int shell_cmd_mkfs(int argc, char **argv) {
    bool auto_mount = false;
    const char *mount_path = NULL;
    if (argc != 3 && argc != 5) {
        tty0_puts("usage: mkfs [fstype] [bdev] [--mount mount-path]\n");
        return 1;
    }
    if (argc == 5) {
        if (strcmp(argv[3], "--mount") != 0) {
            tty0_puts("usage: mkfs [fstype] [bdev] [--mount mount-path]\n");
            return 1;
        }
        auto_mount = true;
        mount_path = argv[4];
    }

    kerrno_t result = OPAL_EUNKNOWN;
    if (strcmp(argv[1], "fat") == 0) {
        struct block_device *dev = NULL;
        if (!parse_bdev_arg("mkfs", argv[2], &dev)) {
            return 1;
        }
        result = mkfs_fat(dev, auto_mount, mount_path);
    } else {
        tty0_printf("mkfs: unsupported fstype '%s'\n", argv[1]);
        return 1;
    }

    if (result != OPAL_OK) {
        tty0_printf("mkfs: error %s (%d)\n", kerrno_str(result), result);
        return 1;
    }

    tty0_puts("mkfs: done\n");
    return 0;
}
