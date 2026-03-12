#ifndef OPAL_FB_FB_H
#define OPAL_FB_FB_H

#include <stdint.h>

typedef uint32_t fb_color_t;

struct fb_tty;

void fb_init(void);
[[nodiscard]] bool fb_is_available(void);
[[nodiscard]] int fb_get_width(void);
[[nodiscard]] int fb_get_height(void);

[[nodiscard]] struct fb_tty *fb_tty_get(void);

void fb_draw_pixel(int x, int y, fb_color_t color);

void fb_fill_rect(int x, int y, int width, int height, fb_color_t color);
void fb_draw_rect(int x, int y, int width, int height, int thickness, fb_color_t color);
void fb_invert_rect(int x, int y, int width, int height);

void fb_draw_char(int x, int y, char c, fb_color_t fg, fb_color_t bg);
void fb_draw_string(int x, int y, const char* str, fb_color_t fg, fb_color_t bg);

void fb_bitblt(int x, int y, int cx, int cy, int x0, int y0);

#endif
