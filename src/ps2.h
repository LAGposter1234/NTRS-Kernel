
#ifndef PS2_H
#define PS2_H

#include <stdint.h>

#define PS2_DATA 0x60
#define PS2_STATUS 0x64
#define PS2_COMMAND 0x64

int mouse_x = 0;
int mouse_y = 0;

static uint8_t ps2_mouse_packet[3];
static uint8_t ps2_mouse_packet_index = 0;

static int ps2_wait_input(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (!(inb(PS2_STATUS) & 2))
            return 1;
    }

    return 0;
}

static int ps2_wait_output(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (inb(PS2_STATUS) & 1)
            return 1;
    }

    return 0;
}

static int ps2_write_command(uint8_t command) {
    if (!ps2_wait_input())
        return 0;

    outb(PS2_COMMAND, command);
    return 1;
}

static int ps2_write_data(uint8_t data) {
    if (!ps2_wait_input())
        return 0;

    outb(PS2_DATA, data);
    return 1;
}

static int ps2_mouse_command(uint8_t command) {
    for (int attempt = 0; attempt < 3; attempt++) {
        if (!ps2_write_command(0xD4))
            return 0;

        if (!ps2_write_data(command))
            return 0;

        for (uint32_t i = 0; i < 100000; i++) {
            uint8_t status = inb(PS2_STATUS);

            if (!(status & 1))
                continue;

            uint8_t response = inb(PS2_DATA);

            if (!(status & 0x20))
                continue;

            if (response == 0xFA)
                return 1;

            if (response != 0xFE)
                return 0;

            break;
        }
    }

    return 0;
}

static int ps2_mouse_init(void) {
    if (!ps2_write_command(0xA8))
        return 0;

    if (!ps2_mouse_command(0xF6))
        return 0;

    if (!ps2_mouse_command(0xF4))
        return 0;

    ps2_mouse_packet_index = 0;
    return 1;
}

uint8_t mouse_lc = 0;
uint8_t mouse_rc = 0;

static void mouse_update(uint8_t sensitivity) {
    while (inb(PS2_STATUS) & 1) {
        uint8_t status = inb(PS2_STATUS);
        uint8_t data = inb(PS2_DATA);

        if (!(status & 0x20))
            continue;

        if (ps2_mouse_packet_index == 0 && !(data & 0x08))
            continue;

        ps2_mouse_packet[ps2_mouse_packet_index++] = data;

        if (ps2_mouse_packet_index == 3) {
            ps2_mouse_packet_index = 0;
            mouse_lc = !!(ps2_mouse_packet[0] & 0x01);
            mouse_rc = !!(ps2_mouse_packet[0] & 0x02);

            if (!(ps2_mouse_packet[0] & 0xC0)) {
                mouse_x += (int8_t)ps2_mouse_packet[1] * sensitivity;
                mouse_y -= (int8_t)ps2_mouse_packet[2] * sensitivity;
                // // if (mouse_x < 0) mouse_x = 0;
                // // if (mouse_y < 0) mouse_y = 0;
                // // if (mouse_x > VGA_WIDTH - 16) mouse_x = VGA_WIDTH -16;
                // // if (mouse_y > VGA_HEIGHT - 16) mouse_y = VGA_HEIGHT -16;
            }
        }
    }
}

#endif
