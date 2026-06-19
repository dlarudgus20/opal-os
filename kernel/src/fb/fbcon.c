#include <stddef.h>

#include <kc/kassert.h>
#include <kc/stdlib.h>

#include <opal/fs/globals.h>

#include "fbcon.h"
#include "kc/kerrno.h"
#include "opal/fs/vfs.h"
#include "opal/mm/kmalloc.h"

#define CW 8
#define CH 16
#define BORDER 1
#define MARGIN 3
#define CUR_THICK 2

#define BORDER_COLOR 15
#define DEFAULT_FG 15
#define DEFAULT_BG 0

enum {
    FBCON_IOCTL_COLOR = 0,
    FBCON_IOCTL_GET_CURSOR = 1,
    FBCON_IOCTL_GOTOXY = 2,
    FBCON_IOCTL_SET_CURSOR_VISIBLE = 3,
    FBCON_IOCTL_PUT_AT = 4,
    FBCON_IOCTL_ERASE_LINE = 5,
    FBCON_IOCTL_SCROLL_UP = 6,
};

struct fbcon_file {
    struct file file;
    struct fbcon *con;
};

static const fb_color_t g_colors[16] = {
    0x0c0c0c,
    0xc50f1f,
    0x13a10e,
    0xc19c00,
    0x0037da,
    0x881798,
    0x3a96dd,
    0xcccccc,
    0x767676,
    0xe74856,
    0x16c60c,
    0xf9f1a5,
    0x3b78ff,
    0xb4009e,
    0x61d6d6,
    0xf2f2f2,
};

static const struct inode_ops g_fbcon_ops;
static const struct file_ops g_fbcon_fops;

static void clear_cursor(struct fbcon *con) {
    if (!con->cursor_visible) {
        return;
    }

    fb_fill_rect(con->xoff + con->xcur * CW + MARGIN,
        con->yoff + (con->ycur + 1) * CH + MARGIN - CUR_THICK, CW, CUR_THICK, con->bg);
}

static void draw_cursor(struct fbcon *con) {
    if (!con->cursor_visible) {
        return;
    }

    fb_fill_rect(con->xoff + con->xcur * CW + MARGIN,
        con->yoff + (con->ycur + 1) * CH + MARGIN - CUR_THICK, CW, CUR_THICK, con->fg);
}

static bool cursor_valid(const struct fbcon *con, int x, int y) {
    return 0 <= x && x < con->width && 0 <= y && y < con->height;
}

static void draw_char_at(struct fbcon *con, int x, int y, char ch) {
    fb_draw_char(con->xoff + x * CW + MARGIN, con->yoff + y * CH + MARGIN, ch, con->fg, con->bg);
}

static void clear_screen(struct fbcon *con) {
    int offset = (MARGIN - BORDER) / 2;
    fb_draw_rect(con->xoff + offset, con->yoff + offset, con->width * CW + (MARGIN - offset) * 2,
        con->height * CH + (MARGIN - offset) * 2, BORDER, g_colors[BORDER_COLOR]);

    offset += BORDER;
    fb_fill_rect(con->xoff + offset, con->yoff + offset, con->width * CW + (MARGIN - offset) * 2,
        con->height * CH + (MARGIN - offset) * 2, con->bg);

    draw_cursor(con);
}

static void scroll_up(struct fbcon *con) {
    clear_cursor(con);
    fb_bitblt(con->xoff + MARGIN, con->yoff + MARGIN, con->width * CW, (con->height - 1) * CH,
        con->xoff + MARGIN, con->yoff + CH + MARGIN);
    fb_fill_rect(con->xoff + MARGIN, con->yoff + (con->height - 1) * CH + MARGIN, con->width * CW,
        CH, con->bg);
    draw_cursor(con);
}

static void write_char(struct fbcon *con, char ch) {
    clear_cursor(con);
    draw_char_at(con, con->xcur, con->ycur, ch);

    if (con->xcur + 1 < con->width) {
        con->xcur += 1;
    }
    draw_cursor(con);
}

static void set_color(struct fbcon *con, int fg, int bg) {
    if (fg == -1 && bg == -1) {
        con->fg = g_colors[DEFAULT_FG];
        con->bg = g_colors[DEFAULT_BG];
        return;
    }

    if (0 <= fg && fg < 16) {
        con->fg = g_colors[fg];
    }
    if (0 <= bg && bg < 16) {
        con->bg = g_colors[bg];
    }
}

void fbcon_init(struct fbcon *con, int x, int y, int width, int height) {
    kassert(con);
    kassert(MARGIN * 2 + CW <= width);
    kassert(MARGIN * 2 + CH <= height);

    int ch_w = (width - MARGIN * 2) / CW;
    int ch_h = (height - MARGIN * 2) / CH;

    con->xoff = x;
    con->yoff = y;
    con->width = ch_w;
    con->height = ch_h;
    con->xcur = 0;
    con->ycur = 0;
    con->cursor_visible = true;

    set_color(con, -1, -1);
    clear_screen(con);

    kobjfs_inode_init(&con->inode, &g_fbcon_ops, "fbcon");
    kobjfs_add_inode(&vfs_globals()->devfs, &con->inode);
}

static void fbcon_inode_close(struct inode *) {
    panic();
}

static kerrno_t fbcon_inode_open(struct inode *inode, enum open_mode mode, struct file **file_out) {
    struct ko_inode *ko = container_of(inode, typeof(*ko), inode);
    struct fbcon *con = container_of(ko, typeof(*con), inode);

    enum open_mode fmode = mode & OPEN_MASK_FMODE;
    if (fmode != OPEN_NONE && fmode != (OPEN_WRITE | OPEN_APPEND)) {
        return OPAL_ENOTSUPP;
    }

    struct fbcon_file *file = kzalloc(sizeof(*file));
    if (!file) {
        return OPAL_ENOMEM;
    }

    file_init(&file->file, &g_fbcon_fops, fmode_from_omode(mode));
    inode_retain(&con->inode.inode);
    file->con = con;

    *file_out = &file->file;
    return OPAL_OK;
}

static kerrno_t fbcon_inode_lookup(struct inode *, struct path_entry *) {
    return OPAL_ENOTDIR;
}

static kerrno_t fbcon_inode_create(struct inode *, struct path_entry *, enum inode_flags) {
    return OPAL_ENOTDIR;
}

static const struct inode_ops g_fbcon_ops = {
    .close = fbcon_inode_close,
    .open = fbcon_inode_open,
    .lookup = fbcon_inode_lookup,
    .create = fbcon_inode_create,
};

static void fbcon_file_close(struct file *base) {
    struct fbcon_file *file = container_of(base, typeof(*file), file);
    inode_release(&file->con->inode.inode);
    kfree(file, sizeof(*file));
}

static fs_ssize_t fbcon_file_seek(struct file *, fs_off_t, enum fs_seek) {
    return OPAL_ENOTSUPP;
}

static fs_ssize_t fbcon_file_read(struct file *, fs_size_t *, void *, fs_size_t) {
    return OPAL_ENOTSUPP;
}

static fs_ssize_t fbcon_file_write(
    struct file *base, fs_size_t *pos, const void *buffer, fs_size_t size) {
    struct fbcon_file *file = container_of(base, typeof(*file), file);
    (void)pos;

    if (!(base->mode & FILE_WRITE)) {
        return OPAL_ENOTSUPP;
    }

    if (size > FS_SSIZE_MAX) {
        return OPAL_EINVAL;
    }

    const char *input = buffer;
    for (fs_size_t idx = 0; idx < size; idx++) {
        write_char(file->con, input[idx]);
    }

    return (fs_ssize_t)size;
}

static kerrno_t fbcon_file_truncate(struct file *, fs_size_t) {
    return OPAL_ENOTSUPP;
}

static kerrno_t fbcon_file_ioctl(struct file *base, uintptr_t op, uintptr_t arg) {
    struct fbcon_file *file = container_of(base, typeof(*file), file);
    struct fbcon *con = file->con;

    switch (op) {
        case FBCON_IOCTL_COLOR:
            if (arg == 0xffff) {
                set_color(con, -1, -1);
            } else {
                int fg = arg & 0xff;
                int bg = (arg >> 8) & 0xff;
                set_color(con, fg == 0xff ? -1 : fg, bg == 0xff ? -1 : bg);
            }
            return OPAL_OK;
        case FBCON_IOCTL_GET_CURSOR:
            return (con->xcur & 0xffff) | ((con->ycur & 0xffff) << 16);
        case FBCON_IOCTL_GOTOXY: {
            int x = arg & 0xffff;
            int y = (arg >> 16) & 0xffff;
            if (!cursor_valid(con, x, y)) {
                return OPAL_ERANGE;
            }
            clear_cursor(con);
            con->xcur = x;
            con->ycur = y;
            draw_cursor(con);
            return OPAL_OK;
        }
        case FBCON_IOCTL_SET_CURSOR_VISIBLE:
            if (arg > 1) {
                return OPAL_EINVAL;
            }
            if (arg == 0) {
                clear_cursor(con);
                con->cursor_visible = false;
            } else {
                con->cursor_visible = true;
                draw_cursor(con);
            }
            return OPAL_OK;
        case FBCON_IOCTL_PUT_AT: {
            int x = arg & 0xff;
            int y = (arg >> 8) & 0xff;
            char ch = (char)((arg >> 16) & 0xff);
            if (!cursor_valid(con, x, y)) {
                return OPAL_ERANGE;
            }
            clear_cursor(con);
            draw_char_at(con, x, y, ch);
            draw_cursor(con);
            return OPAL_OK;
        }
        case FBCON_IOCTL_ERASE_LINE: {
            int x0 = arg & 0xff;
            int y = (arg >> 8) & 0xff;
            int x1 = (arg >> 16) & 0xff;
            if (x0 > x1) {
                return OPAL_EINVAL;
            }
            if (!cursor_valid(con, x0, y) || x1 > con->width) {
                return OPAL_ERANGE;
            }
            clear_cursor(con);
            for (int x = x0; x < x1; x++) {
                draw_char_at(con, x, y, ' ');
            }
            draw_cursor(con);
            return OPAL_OK;
        }
        case FBCON_IOCTL_SCROLL_UP:
            if (arg != 0) {
                return OPAL_EINVAL;
            }
            scroll_up(con);
            return OPAL_OK;
        default:
            return OPAL_ENOTSUPP;
    }
}

static const struct file_ops g_fbcon_fops = {
    .close = fbcon_file_close,
    .seek = fbcon_file_seek,
    .read = fbcon_file_read,
    .write = fbcon_file_write,
    .truncate = fbcon_file_truncate,
    .ioctl = fbcon_file_ioctl,
};
