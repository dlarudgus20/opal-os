#include <stddef.h>

#include <kc/string.h>

#include <opal/tty.h>
#include <opal/klog.h>
#include <opal/hid/hid.h>
#include <opal/fb/fb.h>
#include <opal/tty/fb_tty.h>
#include <opal/locks/irqlock.h>

struct hid_char {
    bool raw;
    char ch;
    hid_keycode_t keycode;
};

struct hid_keyboard_state {
    bool caps;
    bool scroll;
    bool num;
};

struct hid_pointer_state {
    int x;
    int y;
    uint8_t buttons;
};

static bool g_keys[HID_KEYCODE_COUNT];
static struct hid_keyboard_state g_kbd;
static struct hid_pointer_state g_pointer;

static struct hid_char process_keycode(hid_keycode_t key);

static int32_t clamp_i32(int32_t value, int32_t min, int32_t max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static void cursor_draw(int x, int y) {
    fb_invert_rect(x - 2, y - 2, 5, 5);
}

static void cursor_init(void) {
    int width = fb_get_width();
    int height = fb_get_height();
    g_pointer.x = width / 2;
    g_pointer.y = height / 2;
    if (width > 0 && height > 0) {
        cursor_draw(g_pointer.x, g_pointer.y);
    }
}

void hid_init(void) {
    memset(g_keys, 0, sizeof(g_keys));
    memset(&g_kbd, 0, sizeof(g_kbd));
    memset(&g_pointer, 0, sizeof(g_pointer));

    if (fb_is_available()) {
        cursor_init();
    }
}

static void on_key(hid_keycode_t keycode, bool pressed) {
    irqlock_t irqlock = irqlock_acquire();

    g_keys[keycode] = pressed;
    if (pressed) {
        struct hid_char ch = process_keycode(keycode);
        if (!ch.raw) {
            tty0_put_input(&ch.ch, 1);
            if (fb_is_available()) {
                tty_puts_len(&fb_tty_get()->tty, &ch.ch, 1);
            }
        }
    }

    irqlock_release(&irqlock);
}

void hid_report_key(hid_keycode_t keycode, bool pressed) {
    if (keycode < HID_KEYCODE_COUNT) {
        on_key(keycode, pressed);
    } else {
        const char *pressed_str = pressed ? "pressed" : "released";
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
        cursor_draw(x0, y0);
        cursor_draw(x1, y1);
    }
}

static struct hid_char process_keycode(hid_keycode_t key) {
#define RAWKEY(k) (struct hid_char){ .raw = true, .ch = 0, .keycode = (k) }
#define CHARKEY(c) (struct hid_char){ .raw = false, .ch = (c), .keycode = key }
    const struct hid_char raw = RAWKEY(key);

    const bool control = g_keys[HID_KEY_LCTRL] || g_keys[HID_KEY_RCTRL] || g_keys[HID_KEY_LALT]
        || g_keys[HID_KEY_RALT];
    const bool shift = g_keys[HID_KEY_LSHIFT] || g_keys[HID_KEY_RSHIFT];
    const bool upper = g_kbd.caps != shift;

    switch (key) {
        case HID_KEY_CAPSLOCK:
            g_kbd.caps = !g_kbd.caps;
            break;
        case HID_KEY_SCROLLLOCK:
            g_kbd.scroll = !g_kbd.scroll;
            break;
        case HID_KEY_NUMLOCK:
            g_kbd.num = !g_kbd.num;
            break;
        default:
            break;
    }

    if (!control) {
        // clang-format off
        switch (key) {
            case HID_KEY_OEM_TILDE:     return CHARKEY(shift ? '~' : '`');
            case HID_KEY_1:             return CHARKEY(shift ? '!' : '1');
            case HID_KEY_2:             return CHARKEY(shift ? '@' : '2');
            case HID_KEY_3:             return CHARKEY(shift ? '#' : '3');
            case HID_KEY_4:             return CHARKEY(shift ? '$' : '4');
            case HID_KEY_5:             return CHARKEY(shift ? '%' : '5');
            case HID_KEY_6:             return CHARKEY(shift ? '^' : '6');
            case HID_KEY_7:             return CHARKEY(shift ? '&' : '7');
            case HID_KEY_8:             return CHARKEY(shift ? '*' : '8');
            case HID_KEY_9:             return CHARKEY(shift ? '(' : '9');
            case HID_KEY_0:             return CHARKEY(shift ? ')' : '0');
            case HID_KEY_MINUS:         return CHARKEY(shift ? '_' : '-');
            case HID_KEY_PLUS:          return CHARKEY(shift ? '+' : '=');
            case HID_KEY_Q:             return CHARKEY(upper ? 'Q' : 'q');
            case HID_KEY_W:             return CHARKEY(upper ? 'W' : 'w');
            case HID_KEY_E:             return CHARKEY(upper ? 'E' : 'e');
            case HID_KEY_R:             return CHARKEY(upper ? 'R' : 'r');
            case HID_KEY_T:             return CHARKEY(upper ? 'T' : 't');
            case HID_KEY_Y:             return CHARKEY(upper ? 'Y' : 'y');
            case HID_KEY_U:             return CHARKEY(upper ? 'U' : 'u');
            case HID_KEY_I:             return CHARKEY(upper ? 'I' : 'i');
            case HID_KEY_O:             return CHARKEY(upper ? 'O' : 'o');
            case HID_KEY_P:             return CHARKEY(upper ? 'P' : 'p');
            case HID_KEY_OEM_LBRACE:    return CHARKEY(shift ? '{' : '[');
            case HID_KEY_OEM_RBRACE:    return CHARKEY(shift ? '}' : ']');
            case HID_KEY_OEM_PIPE:      return CHARKEY(shift ? '|' : '\\');
            case HID_KEY_A:             return CHARKEY(upper ? 'A' : 'a');
            case HID_KEY_S:             return CHARKEY(upper ? 'S' : 's');
            case HID_KEY_D:             return CHARKEY(upper ? 'D' : 'd');
            case HID_KEY_F:             return CHARKEY(upper ? 'F' : 'f');
            case HID_KEY_G:             return CHARKEY(upper ? 'G' : 'g');
            case HID_KEY_H:             return CHARKEY(upper ? 'H' : 'h');
            case HID_KEY_J:             return CHARKEY(upper ? 'J' : 'j');
            case HID_KEY_K:             return CHARKEY(upper ? 'K' : 'k');
            case HID_KEY_L:             return CHARKEY(upper ? 'L' : 'l');
            case HID_KEY_OEM_COLON:     return CHARKEY(shift ? ':' : ';');
            case HID_KEY_OEM_QUOTE:     return CHARKEY(shift ? '"' : '\'');
            case HID_KEY_Z:             return CHARKEY(upper ? 'Z' : 'z');
            case HID_KEY_X:             return CHARKEY(upper ? 'X' : 'x');
            case HID_KEY_C:             return CHARKEY(upper ? 'C' : 'c');
            case HID_KEY_V:             return CHARKEY(upper ? 'V' : 'v');
            case HID_KEY_B:             return CHARKEY(upper ? 'B' : 'b');
            case HID_KEY_N:             return CHARKEY(upper ? 'N' : 'n');
            case HID_KEY_M:             return CHARKEY(upper ? 'M' : 'm');
            case HID_KEY_COMMA:         return CHARKEY(shift ? '<' : ',');
            case HID_KEY_PERIOD:        return CHARKEY(shift ? '>' : '.');
            case HID_KEY_OEM_SLASH:     return CHARKEY(shift ? '?' : '/');
            case HID_KEY_SPACE:         return CHARKEY(' ');
            case HID_KEY_TAB:           return CHARKEY('\t');
            case HID_KEY_ENTER:         return CHARKEY('\n');

            case HID_KEY_NUMPAD_MUL:    return CHARKEY('*');
            case HID_KEY_NUMPAD_DIV:    return CHARKEY('/');
            case HID_KEY_NUMPAD_ADD:    return CHARKEY('+');
            case HID_KEY_NUMPAD_SUB:    return CHARKEY('-');
            case HID_KEY_NUMPAD_ENTER:  return CHARKEY('\n');

            default: break;
        }
        // clang-format on
    }

    // clang-format off
    switch (key) {
        case HID_KEY_NUMPAD0:       return g_kbd.num ? (control ? raw : CHARKEY('0')) : RAWKEY(HID_KEY_INSERT);
        case HID_KEY_NUMPAD2:       return g_kbd.num ? (control ? raw : CHARKEY('2')) : RAWKEY(HID_KEY_DOWN);
        case HID_KEY_NUMPAD3:       return g_kbd.num ? (control ? raw : CHARKEY('3')) : RAWKEY(HID_KEY_PAGEDOWN);
        case HID_KEY_NUMPAD4:       return g_kbd.num ? (control ? raw : CHARKEY('4')) : RAWKEY(HID_KEY_LEFT);
        case HID_KEY_NUMPAD1:       return g_kbd.num ? (control ? raw : CHARKEY('1')) : RAWKEY(HID_KEY_END);
        case HID_KEY_NUMPAD5:       return g_kbd.num && !control ? CHARKEY('5') : raw;
        case HID_KEY_NUMPAD6:       return g_kbd.num ? (control ? raw : CHARKEY('6')) : RAWKEY(HID_KEY_RIGHT);
        case HID_KEY_NUMPAD7:       return g_kbd.num ? (control ? raw : CHARKEY('7')) : RAWKEY(HID_KEY_HOME);
        case HID_KEY_NUMPAD8:       return g_kbd.num ? (control ? raw : CHARKEY('8')) : RAWKEY(HID_KEY_UP);
        case HID_KEY_NUMPAD9:       return g_kbd.num ? (control ? raw : CHARKEY('9')) : RAWKEY(HID_KEY_PAGEUP);
        case HID_KEY_NUMPAD_PERIOD: return g_kbd.num ? (control ? raw : CHARKEY('.')) : RAWKEY(HID_KEY_DELETE);

        default: break;
    }
    // clang-format on

    return raw;
#undef PS2RAW
#undef PS2CHAR
}
