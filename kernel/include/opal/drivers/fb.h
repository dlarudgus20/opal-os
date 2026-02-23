#ifndef OPAL_DRIVERS_FB_H
#define OPAL_DRIVERS_FB_H

#include <stdint.h>

void fb_init(void);
int fb_get_width(void);
int fb_get_height(void);

void fb_draw_pixel(int x, int y, uint32_t color);

void fb_fill_rect(int x, int y, int width, int height, uint32_t color);
void fb_draw_rect(int x, int y, int width, int height, int thickness, uint32_t color);

void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
void fb_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg);

void fb_bitblt(int x, int y, int cx, int cy, int x0, int y0);

#endif
