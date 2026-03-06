#include <opal/hid/hid.h>
#include <opal/platform/drivers/ps2.h>

static uint8_t g_packet[3];
static uint8_t g_packet_index;

void ps2_mouse_feed_raw(uint8_t data) {
    if (g_packet_index == 0 && (data & 0x08) == 0) {
        return;
    }

    g_packet[g_packet_index++] = data;
    if (g_packet_index < 3) {
        return;
    }

    g_packet_index = 0;

    uint8_t b0 = g_packet[0];
    uint8_t b1 = g_packet[1];
    uint8_t b2 = g_packet[2];

    int16_t dx = (int16_t)(int8_t)b1;
    int16_t dy = (int16_t)(int8_t)b2;
    uint8_t buttons = b0 & 0x07;

    hid_report_pointer(dx, -dy, buttons);
}
