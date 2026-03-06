#include <stddef.h>

#include <kc/string.h>

#include <opal/hid/hid.h>
#include <opal/klog.h>
#include <opal/drivers/fb/fb.h>

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

static void draw_pointer_cursor(void) {
    fb_invert_rect((int)g_pointer.x - 2, (int)g_pointer.y - 2, 5, 5);
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
            draw_pointer_cursor();
        }
    }
}

void hid_report_key(hid_keycode_t keycode, bool pressed) {
    const char *pressed_str = pressed ? "pressed" : "released";
    if (keycode < HID_KEYCODE_COUNT) {
        g_key_pressed[keycode] = pressed;
        kdebug("hid: key %u %s", keycode, pressed_str);
    } else {
        kdebug("hid: unrecognized key %u %s", keycode, pressed_str);
    }
}

void hid_report_pointer(int16_t dx, int16_t dy, uint8_t buttons) {
    if (fb_is_available()) {
        draw_pointer_cursor();

        int x = g_pointer.x + dx;
        int y = g_pointer.y + dy;
        g_pointer.x = clamp_i32(x, 0, fb_get_width() - 1);
        g_pointer.y = clamp_i32(y, 0, fb_get_height() - 1);
        draw_pointer_cursor();
    }

    g_pointer.buttons = buttons & HID_BUTTON_MASK;

    //kdebug("hid: ptr x=%d (%+d) y=%d (%+d) btn=%u",
    //    g_pointer.x, dx, g_pointer.y, dy, g_pointer.buttons);
}
