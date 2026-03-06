#include <stddef.h>

#include <kc/string.h>

#include <opal/klog.h>
#include <opal/hid/hid.h>
#include <opal/fb/fb.h>
#include <opal/locks/irqlock.h>

static bool g_key_pressed[HID_KEYCODE_COUNT];
static struct hid_pointer_state g_pointer;

static int32_t clamp_i32(int32_t value, int32_t min, int32_t max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static void draw_pointer_cursor(int x, int y) {
    fb_invert_rect(x - 2, y - 2, 5, 5);
}

void hid_init(void) {
    memset(g_key_pressed, 0, sizeof(g_key_pressed));
    memset(&g_pointer, 0, sizeof(g_pointer));

    if (fb_is_available()) {
        int width = fb_get_width();
        int height = fb_get_height();
        g_pointer.x = width / 2;
        g_pointer.y = height / 2;
        if (width > 0 && height > 0) {
            draw_pointer_cursor(g_pointer.x, g_pointer.y);
        }
    }
}

void hid_report_key(hid_keycode_t keycode, bool pressed) {
    const char *pressed_str = pressed ? "pressed" : "released";
    if (keycode < HID_KEYCODE_COUNT) {
        irqlock_t irqlock = irqlock_acquire();
        g_key_pressed[keycode] = pressed;
        irqlock_release(&irqlock);

        kdebug("hid: key %u %s", keycode, pressed_str);
    } else {
        kdebug("hid: unrecognized key %u %s", keycode, pressed_str);
    }
}

void hid_report_pointer(int16_t dx, int16_t dy, uint8_t buttons) {
    irqlock_t irqlock = irqlock_acquire();

    int x0 = g_pointer.x;
    int y0 = g_pointer.y;
    g_pointer.buttons = buttons & HID_BUTTON_MASK;

    if (fb_is_available()) {
        int x = g_pointer.x + dx;
        int y = g_pointer.y + dy;
        g_pointer.x = clamp_i32(x, 0, fb_get_width() - 1);
        g_pointer.y = clamp_i32(y, 0, fb_get_height() - 1);
    }

    int x1 = g_pointer.x;
    int y1 = g_pointer.y;

    irqlock_release(&irqlock);

    if (fb_is_available() && (x0 != x1 || y0 != y1)) {
        draw_pointer_cursor(x0, y0);
        draw_pointer_cursor(x1, y1);
    }
}
