#pragma once
#include <stdint.h>

#define SB16_BASE       0x220
#define SB16_RESET      (SB16_BASE + 0x06)
#define SB16_READ       (SB16_BASE + 0x0A)
#define SB16_WRITE      (SB16_BASE + 0x0C)
#define SB16_READ_STATUS (SB16_BASE + 0x0E)

static inline void sb16_outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t sb16_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static int sb16_wait_write(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (!(sb16_inb(SB16_WRITE) & 0x80))
            return 1;
    }
    return 0;
}

static int sb16_wait_read(void) {
    for (uint32_t i = 0; i < 100000; i++) {
        if (sb16_inb(SB16_READ_STATUS) & 0x80)
            return 1;
    }
    return 0;
}

static int sb16_write(uint8_t value) {
    if (!sb16_wait_write())
        return 0;

    sb16_outb(SB16_WRITE, value);
    return 1;
}

static int sb16_init(void) {
    sb16_outb(SB16_RESET, 1);

    for (volatile uint32_t i = 0; i < 10000; i++)
        __asm__ volatile ("pause");

    sb16_outb(SB16_RESET, 0);

    if (!sb16_wait_read())
        return 0;

    if (sb16_inb(SB16_READ) != 0xAA)
        return 0;

    return 1;
}

static int sb16_get_version(uint8_t *major, uint8_t *minor) {
    if (!sb16_write(0xE1))
        return 0;

    if (!sb16_wait_read())
        return 0;
    *major = sb16_inb(SB16_READ);

    if (!sb16_wait_read())
        return 0;
    *minor = sb16_inb(SB16_READ);

    return 1;
}


static int sb16_set_rate(uint16_t rate) {
    if (!sb16_write(0x41))
        return 0;
    if (!sb16_write(rate >> 8))
        return 0;
    return sb16_write(rate & 0xFF);
}

static int sb16_dma_start(uint32_t phys, uint16_t length) {
    if (!length || phys >= 0x1000000)
        return 0;

    if ((phys & 0xFFFF) + length > 0x10000)
        return 0;

    sb16_outb(0x0A, 0x05);
    sb16_outb(0x0C, 0);
    sb16_outb(0x02, phys & 0xFF);
    sb16_outb(0x02, (phys >> 8) & 0xFF);
    sb16_outb(0x83, (phys >> 16) & 0xFF);
    sb16_outb(0x0C, 0);
    sb16_outb(0x03, (length - 1) & 0xFF);
    sb16_outb(0x03, (length - 1) >> 8);
    sb16_outb(0x0B, 0x49);
    sb16_outb(0x0A, 0x01);

    return 1;
}

static int sb16_play8(const uint8_t *samples, uint32_t phys,
                      uint16_t length, uint16_t rate) {
    if (!samples || !length || !rate)
        return 0;

    if (!sb16_dma_start(phys, length))
        return 0;

    if (!sb16_set_rate(rate))
        return 0;

    if (!sb16_write(0xC0) ||
        !sb16_write(0x00) ||
        !sb16_write((length - 1) & 0xFF) ||
        !sb16_write((length - 1) >> 8))
        return 0;

    return 1;
 }

