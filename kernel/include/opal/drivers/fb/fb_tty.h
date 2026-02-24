#ifndef OPAL_DRIVERS_FB_FB_TTY_H
#define OPAL_DRIVERS_FB_FB_TTY_H

#include <opal/tty.h>
#include <opal/drivers/fb/fb.h>

struct fb_tty {
    struct tty tty;

    int xoff;
    int yoff;
    int width;
    int height;

    int xcur;
    int ycur;

    fb_color_t fg;
    fb_color_t bg;
};

void fb_tty_init(struct fb_tty *tty, int x, int y, int width, int height);

#endif
