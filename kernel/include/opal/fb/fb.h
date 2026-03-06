#ifndef OPAL_FB_FB_H
#define OPAL_FB_FB_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t fb_color_t;

void fb_init(void);
bool fb_is_available(void);
int fb_get_width(void);
int fb_get_height(void);

void fb_draw_pixel(int x, int y, fb_color_t color);

void fb_fill_rect(int x, int y, int width, int height, fb_color_t color);
void fb_draw_rect(int x, int y, int width, int height, int thickness, fb_color_t color);
void fb_invert_rect(int x, int y, int width, int height);

void fb_draw_char(int x, int y, char c, fb_color_t fg, fb_color_t bg);
void fb_draw_string(int x, int y, const char* str, fb_color_t fg, fb_color_t bg);

void fb_bitblt(int x, int y, int cx, int cy, int x0, int y0);

#endif
