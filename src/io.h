#pragma once
#include <stdint.h>
#include <stdbool.h>

static inline void outb(uint16_t port, uint8_t value){
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}
static inline uint16_t inw(uint16_t port) {
    uint16_t value;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline uint32_t inl(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %w1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %w1" :: "a"(value), "Nd"(port));
}

static const char keymap[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\', [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c',
    [0x2F] = 'v', [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/', [0x39] = ' '
};

#define KEY_UP       0x148
#define KEY_DOWN     0x150
#define KEY_LEFT     0x14B
#define KEY_RIGHT    0x14D
#define KEY_HOME     0x147
#define KEY_END      0x14F
#define KEY_DELETE   0x153
#define KEY_PAGEUP   0x149
#define KEY_PAGEDOWN 0x151

static inline uint16_t get_key(void) {
    static uint8_t extended = 0;

    for (;;) {
        if (!(inb(0x64) & 1)) continue;

        uint8_t sc = inb(0x60);

        if (sc == 0xE0) {
            extended = 1;
            continue;
        }

        if (sc == 0xE1) {
            extended = 0;
            continue;
        }

        if (sc & 0x80) {
            extended = 0;
            continue;
        }

        if (extended) {
            extended = 0;
            return 0x100 | sc;
        }

        if (keymap[sc]) return keymap[sc];
    }
}

static inline uint16_t get_key_noblock(void) {
    static uint8_t extended = 0;
    uint8_t status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) return 0;

    uint8_t sc = inb(0x60);

    if (sc == 0xE0) {
        extended = 1;
        return 0;
    }

    if (sc & 0x80) {
        extended = 0;
        return 0;
    }

    if (extended) {
        extended = 0;
        switch (sc) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
            default: return 0;
        }
    }

    return keymap[sc];
}

static void *memset(void *dst, int value, uint64_t size) {
    uint8_t *d = dst;
    for (uint64_t i = 0; i < size; i++) d[i] = (uint8_t)value;
    return dst;
}

static void *memset32(void *dst, int value, uint64_t size) {
    uint32_t *d = dst;
    for (uint64_t i = 0; i < size; i++) d[i] = (uint32_t)value;
    return dst;
}

static void *memcpy(void *dst, const void *src, uint64_t size) {
    uint8_t *d = dst;
    const uint8_t *s = src;
    for (uint64_t i = 0; i < size; i++) d[i] = s[i];
    return dst;
}

static void *memmove(void *dst, const void *src, uint64_t size) {
    uint8_t *d = dst;
    const uint8_t *s = src;

    if (d < s) {
        for (uint64_t i = 0; i < size; i++) d[i] = s[i];
    } else if (d > s) {
        for (uint64_t i = size; i > 0; i--) d[i - 1] = s[i - 1];
    }

    return dst;
}

static int memcmp(const void *a, const void *b, uint64_t size) {
    const uint8_t *x = a;
    const uint8_t *y = b;

    for (uint64_t i = 0; i < size; i++) {
        if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    }

    return 0;
}

char *strcpy(char *dest, const char *src) {
    char *start = dest;
    while ((*dest++ = *src++));
    return start;
}

int strcmp(char *a, char *b) {
    while (*a == *b) {
        if (!*a) return 0;
        a++;
        b++;
    }
    return 1;
}
