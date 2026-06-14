#ifndef FB_FBCON_H
#define FB_FBCON_H

#include <opal/fb/fb.h>
#include <opal/fs/kobjfs.h>

struct fbcon {
    struct ko_inode inode;

    int xoff;
    int yoff;
    int width;
    int height;

    int xcur;
    int ycur;

    fb_color_t fg;
    fb_color_t bg;
};

void fbcon_init(struct fbcon *con, int x, int y, int width, int height);

#endif
