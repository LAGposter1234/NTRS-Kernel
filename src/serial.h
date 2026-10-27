#pragma once
#define SERIAL_PORT 0x3F8

static inline void serial_outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t serial_inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

void serial_init(void) {
    serial_outb(SERIAL_PORT + 1, 0x00);
    serial_outb(SERIAL_PORT + 3, 0x80);
    serial_outb(SERIAL_PORT + 0, 0x03);
    serial_outb(SERIAL_PORT + 1, 0x00);
    serial_outb(SERIAL_PORT + 3, 0x03);
    serial_outb(SERIAL_PORT + 2, 0xC7);
    serial_outb(SERIAL_PORT + 4, 0x0B);
}

void serial_putchar(char c) {
    while (!(serial_inb(SERIAL_PORT + 5) & 0x20)) {}
    serial_outb(SERIAL_PORT, c);
}

void log(const char *s) {
    while (*s) {
        if (*s == '\n') serial_putchar('\r');
        serial_putchar(*s++);
    }
}

void log_hex(const char *label, uint32_t value) {
    static const char hex[] = "0123456789ABCDEF";
    char buf[11] = "0x00000000";

    for (int i = 0; i < 8; i++)
        buf[i + 2] = hex[(value >> (28 - i * 4)) & 0xF];

    log(label);
    log(buf);
}
