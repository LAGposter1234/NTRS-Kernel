#pragma once
#include <stdint.h>

#define VGA_WIDTH 640
#define VGA_HEIGHT 480
#define VGA_PIXELS (VGA_WIDTH * VGA_HEIGHT)
#define VGA_MEMORY ((volatile uint8_t *)0xA0000)

static uint8_t *backbuffer;

static inline void vga_out(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

void putpixel(uint8_t color, int x, int y) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT)
        return;
    backbuffer[y * VGA_WIDTH + x] = color & 0x0F;
}

void clear(uint8_t color) {
    for (uint32_t i = 0; i < VGA_PIXELS; i++)
        backbuffer[i] = color & 0x0F;
}
void flush(void) {
    vga_out(0x3C4, 0x04);
    vga_out(0x3C5, 0x06);

    vga_out(0x3CE, 0x06);
    vga_out(0x3CF, 0x05);

    vga_out(0x3CE, 0x03);
    vga_out(0x3CF, 0x00);

    vga_out(0x3CE, 0x05);
    vga_out(0x3CF, 0x00);

    vga_out(0x3CE, 0x08);
    vga_out(0x3CF, 0xFF);

    for (uint8_t plane = 0; plane < 4; plane++) {
        vga_out(0x3C4, 0x02);
        vga_out(0x3C5, 1 << plane);

        for (uint32_t i = 0; i < VGA_PIXELS; i += 8) {
            uint8_t packed = 0;
            for (uint32_t bit = 0; bit < 8; bit++)
                packed |= ((backbuffer[i + bit] >> plane) & 1) << (7 - bit);
            VGA_MEMORY[i / 8] = packed;
        }
    }

    vga_out(0x3C4, 0x02);
    vga_out(0x3C5, 0x0F);
}

void vga_init(void) {
    vga_out(0x3C4, 0x04);
    vga_out(0x3C5, 0x06);

    vga_out(0x3CE, 0x05);
    vga_out(0x3CF, 0x00);

    vga_out(0x3CE, 0x06);
    vga_out(0x3CF, 0x05);

    vga_out(0x3CE, 0x08);
    vga_out(0x3CF, 0xFF);
    backbuffer = (uint8_t*)0xE00000;
}

void draw_char(char c, uint8_t color, int x, int y) {
    if ((uint8_t)c < 32 || (uint8_t)c > 126) return;
    const uint8_t *glyph = (const uint8_t *)&Font[((uint8_t)c - 32) * 5];

    for (int col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (int row = 0; row < 5; row++) {
            if (bits & (1u << row))
                putpixel(color, x + col, y + row);
        }
    }
}
