#include <kc/kassert.h>

#include <opal/tty/fb_tty.h>

#define CW 8
#define CH 16
#define BORDER 1
#define MARGIN 3
#define CUR_THICK 2

#define BORDER_COLOR 15
#define DEFAULT_FG 15
#define DEFAULT_BG 0

static fb_color_t g_colors[16] = {
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

static void draw_cursor(struct fb_tty *tty) {
    fb_fill_rect(tty->xoff + tty->xcur * CW + MARGIN,
        tty->yoff + (tty->ycur + 1) * CH + MARGIN - CUR_THICK, CW, CUR_THICK, tty->fg);
}

static void clear_screen(struct fb_tty *tty) {
    int offset = (MARGIN - BORDER) / 2;
    fb_draw_rect(tty->xoff + offset, tty->yoff + offset, tty->width * CW + (MARGIN - offset) * 2,
        tty->height * CH + (MARGIN - offset) * 2, BORDER, g_colors[BORDER_COLOR]);

    offset += BORDER;
    fb_fill_rect(tty->xoff + offset, tty->yoff + offset, tty->width * CW + (MARGIN - offset) * 2,
        tty->height * CH + (MARGIN - offset) * 2, tty->bg);

    draw_cursor(tty);
}

static void scroll(struct fb_tty *tty) {
    if (tty->xcur < tty->width) {
        fb_fill_rect(tty->xoff + tty->xcur * CW + MARGIN, tty->yoff + tty->ycur * CH + MARGIN, CW,
            CH, tty->bg);
    }
    tty->xcur = 0;

    if (tty->ycur + 1 < tty->height) {
        tty->ycur++;
        draw_cursor(tty);
        return;
    }

    fb_bitblt(tty->xoff + MARGIN, tty->yoff + MARGIN, tty->width * CW, (tty->height - 1) * CH,
        tty->xoff + MARGIN, tty->yoff + CH + MARGIN);
    fb_fill_rect(tty->xoff + MARGIN, tty->yoff + (tty->height - 1) * CH + MARGIN, tty->width * CW,
        CH, tty->bg);
    draw_cursor(tty);
}

static void erase(struct fb_tty *tty) {
    if (tty->xcur == 0) {
        return;
    }

    tty->xcur -= 1;
    fb_fill_rect(tty->xoff + tty->xcur * CW + MARGIN, tty->yoff + tty->ycur * CH + MARGIN, CW * 2,
        CH, tty->bg);
    draw_cursor(tty);
}

static void write_char(struct tty *tty_, char ch) {
    struct fb_tty *tty = (struct fb_tty *)tty_;

    if (ch == '\n') {
        scroll(tty);
        return;
    }

    if (ch == '\b') {
        erase(tty);
        return;
    }

    fb_draw_char(tty->xoff + tty->xcur * CW + MARGIN, tty->yoff + tty->ycur * CH + MARGIN, ch,
        tty->fg, tty->bg);

    tty->xcur += 1;
    if (tty->xcur >= tty->width) {
        scroll(tty);
    } else {
        draw_cursor(tty);
    }
}

static size_t write_tty(struct tty *tty_, const char *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        write_char(tty_, buf[i]);
    }
    return len;
}

static void set_color(struct tty *tty_, int fg, int bg) {
    struct fb_tty *tty = (struct fb_tty *)tty_;

    if (fg == -1 && bg == -1) {
        tty->fg = g_colors[DEFAULT_FG];
        tty->bg = g_colors[DEFAULT_BG];
        return;
    }

    if (0 <= fg && fg < 16) {
        tty->fg = g_colors[fg];
    }
    if (0 <= bg && bg < 16) {
        tty->bg = g_colors[bg];
    }
}

static struct tty_ops g_tty_ops = {
    .write = write_tty,
    .set_color = set_color,
};

void fb_tty_init(struct fb_tty *tty, int x, int y, int width, int height) {
    kassert(tty);
    kassert(MARGIN * 2 + CW <= width);
    kassert(MARGIN * 2 + CH <= height);

    int ch_w = (width - MARGIN * 2) / CW;
    int ch_h = (height - MARGIN * 2) / CH;

    tty_init(&tty->tty, &g_tty_ops);

    tty->xoff = x;
    tty->yoff = y;
    tty->width = ch_w;
    tty->height = ch_h;
    tty->xcur = 0;
    tty->ycur = 0;

    set_color(&tty->tty, -1, -1);
    clear_screen(tty);
}
