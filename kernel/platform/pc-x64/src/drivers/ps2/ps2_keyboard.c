#include <opal/hid/hid.h>
#include <opal/platform/drivers/ps2.h>

#define SC_EXT1         0xe0
#define SC_EXT2         0xe1
#define SC_PRTSCR_DOWN1 0x2a
#define SC_PRTSCR_DOWN2 0x37
#define SC_PRTSCR_UP1   0xb7
#define SC_PRTSCR_UP2   0xaa
#define SC_PAUSE_DOWN   0x1d45
#define SC_PAUSE_UP     0x9dc5

enum {
    STATE_START,
    STATE_EXT,
    STATE_PRTSCR1,
    STATE_PRTSCR2,
    STATE_PAUSE1,
    STATE_PAUSE2,
};

static uint8_t g_state = STATE_START;
static uint8_t g_prev;

static hid_keycode_t keycode_normal(uint8_t byte) {
    // clang-format off
    switch (byte) {
        case 0x01: return HID_KEY_ESCAPE;
        case 0x02: return HID_KEY_1;
        case 0x03: return HID_KEY_2;
        case 0x04: return HID_KEY_3;
        case 0x05: return HID_KEY_4;
        case 0x06: return HID_KEY_5;
        case 0x07: return HID_KEY_6;
        case 0x08: return HID_KEY_7;
        case 0x09: return HID_KEY_8;
        case 0x0a: return HID_KEY_9;
        case 0x0b: return HID_KEY_0;
        case 0x0c: return HID_KEY_MINUS;
        case 0x0d: return HID_KEY_PLUS;
        case 0x0e: return HID_KEY_BACKSPACE;
        case 0x0f: return HID_KEY_TAB;
        case 0x10: return HID_KEY_Q;
        case 0x11: return HID_KEY_W;
        case 0x12: return HID_KEY_E;
        case 0x13: return HID_KEY_R;
        case 0x14: return HID_KEY_T;
        case 0x15: return HID_KEY_Y;
        case 0x16: return HID_KEY_U;
        case 0x17: return HID_KEY_I;
        case 0x18: return HID_KEY_O;
        case 0x19: return HID_KEY_P;
        case 0x1a: return HID_KEY_OEM_LBRACE;
        case 0x1b: return HID_KEY_OEM_RBRACE;
        case 0x1c: return HID_KEY_ENTER;
        case 0x1d: return HID_KEY_LCTRL;
        case 0x1e: return HID_KEY_A;
        case 0x1f: return HID_KEY_S;
        case 0x20: return HID_KEY_D;
        case 0x21: return HID_KEY_F;
        case 0x22: return HID_KEY_G;
        case 0x23: return HID_KEY_H;
        case 0x24: return HID_KEY_J;
        case 0x25: return HID_KEY_K;
        case 0x26: return HID_KEY_L;
        case 0x27: return HID_KEY_OEM_COLON;
        case 0x28: return HID_KEY_OEM_QUOTE;
        case 0x29: return HID_KEY_OEM_TILDE;
        case 0x2a: return HID_KEY_LSHIFT;
        case 0x2b: return HID_KEY_OEM_PIPE;
        case 0x2c: return HID_KEY_Z;
        case 0x2d: return HID_KEY_X;
        case 0x2e: return HID_KEY_C;
        case 0x2f: return HID_KEY_V;
        case 0x30: return HID_KEY_B;
        case 0x31: return HID_KEY_N;
        case 0x32: return HID_KEY_M;
        case 0x33: return HID_KEY_COMMA;
        case 0x34: return HID_KEY_PERIOD;
        case 0x35: return HID_KEY_OEM_SLASH;
        case 0x36: return HID_KEY_RSHIFT;
        case 0x37: return HID_KEY_NUMPAD_MUL;
        case 0x38: return HID_KEY_LALT;
        case 0x39: return HID_KEY_SPACE;
        case 0x3a: return HID_KEY_CAPSLOCK;
        case 0x3b: return HID_KEY_F1;
        case 0x3c: return HID_KEY_F2;
        case 0x3d: return HID_KEY_F3;
        case 0x3e: return HID_KEY_F4;
        case 0x3f: return HID_KEY_F5;
        case 0x40: return HID_KEY_F6;
        case 0x41: return HID_KEY_F7;
        case 0x42: return HID_KEY_F8;
        case 0x43: return HID_KEY_F9;
        case 0x44: return HID_KEY_F10;
        case 0x45: return HID_KEY_NUMLOCK;
        case 0x46: return HID_KEY_SCROLLLOCK;
        case 0x47: return HID_KEY_NUMPAD7;
        case 0x48: return HID_KEY_NUMPAD8;
        case 0x49: return HID_KEY_NUMPAD9;
        case 0x4a: return HID_KEY_NUMPAD_SUB;
        case 0x4b: return HID_KEY_NUMPAD4;
        case 0x4c: return HID_KEY_NUMPAD5;
        case 0x4d: return HID_KEY_NUMPAD6;
        case 0x4e: return HID_KEY_NUMPAD_ADD;
        case 0x4f: return HID_KEY_NUMPAD1;
        case 0x50: return HID_KEY_NUMPAD2;
        case 0x51: return HID_KEY_NUMPAD3;
        case 0x52: return HID_KEY_NUMPAD0;
        case 0x53: return HID_KEY_NUMPAD_PERIOD;
        case 0x57: return HID_KEY_F11;
        case 0x58: return HID_KEY_F12;
        default: return HID_KEYCODE_UNKNOWN;
    }
    // clang-format on
}

static hid_keycode_t keycode_extended(uint8_t byte) {
    // clang-format off
    switch (byte) {
        case 0x10: return HID_KEY_MM_PREVTRACK;
        case 0x19: return HID_KEY_MM_NEXTTRACK;
        case 0x1c: return HID_KEY_NUMPAD_ENTER;
        case 0x1d: return HID_KEY_RCTRL;
        case 0x20: return HID_KEY_MM_MUTE;
        case 0x21: return HID_KEY_MM_CALCULATOR;
        case 0x22: return HID_KEY_MM_PLAYPAUSE;
        case 0x24: return HID_KEY_MM_STOP;
        case 0x2e: return HID_KEY_MM_VOLUMEDOWN;
        case 0x30: return HID_KEY_MM_VOLUMEUP;
        case 0x32: return HID_KEY_MM_WWWHOME;
        case 0x35: return HID_KEY_NUMPAD_DIV;
        case 0x38: return HID_KEY_RALT;
        case 0x47: return HID_KEY_HOME;
        case 0x48: return HID_KEY_UP;
        case 0x49: return HID_KEY_PAGEUP;
        case 0x4b: return HID_KEY_LEFT;
        case 0x4d: return HID_KEY_RIGHT;
        case 0x4f: return HID_KEY_END;
        case 0x50: return HID_KEY_DOWN;
        case 0x51: return HID_KEY_PAGEDOWN;
        case 0x52: return HID_KEY_INSERT;
        case 0x53: return HID_KEY_DELETE;
        case 0x5b: return HID_KEY_LWIN;
        case 0x5c: return HID_KEY_RWIN;
        case 0x5d: return HID_KEY_APPS;
        case 0x5e: return HID_KEY_ACPI_POWER;
        case 0x5f: return HID_KEY_ACPI_SLEEP;
        case 0x63: return HID_KEY_ACPI_WAKE;
        case 0x65: return HID_KEY_MM_WWWSEARCH;
        case 0x66: return HID_KEY_MM_WWWFAVORITES;
        case 0x67: return HID_KEY_MM_WWWREFRESH;
        case 0x68: return HID_KEY_MM_WWWSTOP;
        case 0x69: return HID_KEY_MM_WWWFORWARD;
        case 0x6a: return HID_KEY_MM_WWWBACK;
        case 0x6b: return HID_KEY_MM_MYCOMPUTER;
        case 0x6c: return HID_KEY_MM_EMAIL;
        case 0x6d: return HID_KEY_MM_MEDIASELECT;
        default: return HID_KEYCODE_UNKNOWN;
    }
    // clang-format on
}

void ps2_keyboard_feed_raw(uint8_t data) {
    switch (g_state) {
        case STATE_START:
            if (data == SC_EXT1) {
                g_state = STATE_EXT;
            } else if (data == SC_EXT2) {
                g_state = STATE_PAUSE1;
            } else {
                uint8_t scancode = data & 0x7f;
                bool pressed = (data & 0x80) == 0;
                hid_keycode_t key = keycode_normal(scancode);
                hid_report_key(key, pressed);
            }
            break;
        case STATE_EXT:
            if (data == SC_PRTSCR_DOWN1 || data == SC_PRTSCR_UP1) {
                g_state = STATE_PRTSCR1;
                g_prev = data;
            } else {
                g_state = STATE_START;
                uint8_t scancode = data & 0x7f;
                bool pressed = (data & 0x80) == 0;
                hid_keycode_t key = keycode_extended(scancode);
                hid_report_key(key, pressed);
            }
            break;
        case STATE_PRTSCR1:
            if (data == SC_EXT1) {
                g_state = STATE_PRTSCR2;
            } else {
                // something goes wrong; reset state
                g_state = STATE_START;
            }
            break;
        case STATE_PRTSCR2:
            g_state = STATE_START;
            if (g_prev == SC_PRTSCR_DOWN1 && data == SC_PRTSCR_DOWN2) {
                hid_report_key(HID_KEY_PRINTSCREEN, true);
            } else if (g_prev == SC_PRTSCR_UP1 && data == SC_PRTSCR_UP2) {
                hid_report_key(HID_KEY_PRINTSCREEN, false);
            } else {
                // unknown key
            }
            break;
        case STATE_PAUSE1:
            g_state = STATE_PAUSE2;
            g_prev = data;
            break;
        case STATE_PAUSE2:
            g_state = STATE_START;
            uint16_t key = (g_prev << 8) | data;
            if (key == SC_PAUSE_DOWN) {
                hid_report_key(HID_KEY_PAUSEBREAK, true);
            } else if (key == SC_PAUSE_UP) {
                hid_report_key(HID_KEY_PAUSEBREAK, false);
            } else {
                // unknown key
            }
            break;
    }
}
