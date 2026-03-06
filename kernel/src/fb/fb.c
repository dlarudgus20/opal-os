#include <kc/attributes.h>
#include <kc/stdlib.h>

#include <opal/mm/vmap.h>
#include <opal/fb/fb.h>
#include <opal/tty/fb_tty.h>
#include <opal/platform/boot.h>
#include <opal/platform/mm/defines.h>

extern unsigned char g_font_data[];

// skip psf header
#define g_ascii_font (g_font_data + 32)

struct fbinfo {
    volatile uint32_t *fb;
    uint32_t pitch;
    int width;
    int height;
};

static struct fbinfo g_fb;
static struct fb_tty g_fb_tty;

void fb_init(void) {
    const struct boot_fbinfo *fbinfo = boot_get_fbinfo();
    if (!fbinfo) {
        return;
    }

    if (fbinfo->pitch % sizeof(uint32_t) != 0
        || fbinfo->width > INT_MAX || fbinfo->height > INT_MAX
    ) {
        // kwarn("framebuffer configuration is broken");
        return;
    }

    void *fb_ptr;
    struct span fb = mm_vmap_alloc(&fb_ptr, fbinfo->addr, (phys_size_t)fbinfo->pitch * fbinfo->height);
    if (!fb.ptr) {
        // kwarn("cannot allocate page table for framebuffer");
        return;
    }

    g_fb.fb = fb_ptr;
    g_fb.pitch = fbinfo->pitch / sizeof(uint32_t);
    g_fb.width = (int)fbinfo->width;
    g_fb.height = (int)fbinfo->height;

    fb_tty_init(&g_fb_tty, 0, 0, g_fb.width, g_fb.height);
    tty0_register(&g_fb_tty.tty);
}

bool fb_is_available(void) {
    return g_fb.fb != NULL;
}

int fb_get_width(void) {
    return g_fb.width;
}

int fb_get_height(void) {
    return g_fb.height;
}

static bool is_exist(void) {
    return g_fb.fb;
}

static bool is_valid(int x, int y) {
    return 0 <= x && x < g_fb.width
        && 0 <= y && y < g_fb.height;
}

void fb_draw_pixel(int x, int y, uint32_t color) {
    if (is_exist() && is_valid(x, y)) {
        g_fb.fb[x + y * g_fb.pitch] = color & 0xffffff;
    }
}

void fb_fill_rect(int x, int y, int width, int height, uint32_t color) {
    for (int yi = 0; yi < height; yi++) {
        for (int xi = 0; xi < width; xi++) {
            fb_draw_pixel(x + xi, y + yi, color);
        }
    }
}

void fb_draw_rect(int x, int y, int width, int height, int thickness, uint32_t color) {
    fb_fill_rect(x, y, width, thickness, color);
    fb_fill_rect(x, y + height - thickness, width, thickness, color);
    fb_fill_rect(x, y + thickness, thickness, height - 2 * thickness, color);
    fb_fill_rect(x + width - thickness, y + thickness, thickness, height - 2 * thickness, color);
}

void fb_invert_rect(int x, int y, int width, int height) {
    if (!is_exist() || width <= 0 || height <= 0) {
        return;
    }

    int x0 = x;
    int y0 = y;
    int x1 = x + width;
    int y1 = y + height;

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > g_fb.width) {
        x1 = g_fb.width;
    }
    if (y1 > g_fb.height) {
        y1 = g_fb.height;
    }

    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int yi = y0; yi < y1; yi++) {
        for (int xi = x0; xi < x1; xi++) {
            g_fb.fb[xi + yi * g_fb.pitch] ^= 0x00ffffffu;
        }
    }
}

void fb_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    unsigned char index = (unsigned char)c;
    if (index >= 128) {
        index = 0;
    }

    unsigned char *font = g_ascii_font + index * 16;

    for (int yi = 0; yi < 16; yi++) {
        for (int xi = 0; xi < 8; xi++) {
            if (font[yi] & (0x80 >> xi)) {
                fb_draw_pixel(x + xi, y + yi, fg);
            } else if (!(bg & 0xff000000)) {
                fb_draw_pixel(x + xi, y + yi, bg);
            }
        }
    }
}

void fb_draw_string(int x, int y, const char* str, uint32_t fg, uint32_t bg) {
    while (*str != '\0') {
        fb_draw_char(x, y, *str, fg, bg);
        x += 8;
        str++;
    }
}

struct rect {
    int x, y, width, height;
};

struct rect rect_intersect(const struct rect* r1, const struct rect* r2) {
    struct rect r;
    r.x = MAX(r1->x, r2->x);
    r.y = MAX(r1->y, r2->y);
    r.width = MIN(r1->x + r1->width, r2->x + r2->width) - r.x;
    r.height = MIN(r1->y + r1->height, r2->y + r2->height) - r.y;
    if (r.width < 0 || r.height < 0) {
        r.width = 0;
        r.height = 0;
    }
    return r;
}

#if defined(DEBUG) && __has_attribute(optimize) && __has_attribute(no_sanitize)
[[gnu::optimize("-O3"), gnu::no_sanitize("undefined")]]
#endif
void fb_bitblt(int x, int y, int cx, int cy, int x0, int y0) {
    if (!is_exist() || cx <= 0 || cy <= 0) {
        return;
    }

    // clip [x, y, cx, cy] and [x0, y0, cx, cy] into screen
    if (x < 0) {
        x0 += -x;
        cx -= -x;
        x = 0;
    }
    if (y < 0) {
        y0 += -y;
        cy -= -y;
        y = 0;
    }
    if (x0 < 0) {
        x += -x0;
        cx -= -x0;
        x0 = 0;
    }
    if (y0 < 0) {
        y += -y0;
        cy -= -y0;
        y0 = 0;
    }

    if (cx <= 0 || cy <= 0) {
        return;
    }

    if (x >= g_fb.width || y >= g_fb.height
        || x0 >= g_fb.width || y0 >= g_fb.height
    ) {
        return;
    }

    if (x + cx > g_fb.width) {
        cx = g_fb.width - x;
    }
    if (x0 + cx > g_fb.width) {
        cx = g_fb.width - x0;
    }
    if (y + cy > g_fb.height) {
        cy = g_fb.height - y;
    }
    if (y0 + cy > g_fb.height) {
        cy = g_fb.height - y0;
    }

    if (cx <= 0 || cy <= 0) {
        return;
    }

    // memmove
    if (y < y0) {
        for (int row = 0; row < cy; row++) {
            volatile uint32_t *dst = g_fb.fb + x + (y + row) * g_fb.pitch;
            volatile uint32_t *src = g_fb.fb + x0 + (y0 + row) * g_fb.pitch;
            for (int col = 0; col < cx; col++) {
                dst[col] = src[col];
            }
        }
    } else if (y > y0) {
        for (int row = cy - 1; row >= 0; row--) {
            volatile uint32_t *dst = g_fb.fb + x + (y + row) * g_fb.pitch;
            volatile uint32_t *src = g_fb.fb + x0 + (y0 + row) * g_fb.pitch;
            for (int col = 0; col < cx; col++) {
                dst[col] = src[col];
            }
        }
    } else {
        for (int row = 0; row < cy; row++) {
            volatile uint32_t *dst = g_fb.fb + x + (y + row) * g_fb.pitch;
            volatile uint32_t *src = g_fb.fb + x0 + (y0 + row) * g_fb.pitch;

            if (x < x0) {
                for (int col = 0; col < cx; col++) {
                    dst[col] = src[col];
                }
            } else {
                for (int col = cx - 1; col >= 0; col--) {
                    dst[col] = src[col];
                }
            }
        }
    }
}
