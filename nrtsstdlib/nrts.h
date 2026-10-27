#ifndef NRTS_H
#define NRTS_H

#include <stdint.h>

static inline void getln(char *buffer) {
    __asm__ volatile (
        "int $0x23"
        :
        : "D"(buffer), "S"((uint64_t)128)
        : "memory"
    );
}

static inline void puts(const char *str) {
    __asm__ volatile ("int $0x22" : : "D"(str) : "memory");
}


static inline int read(const char *path, void *buffer, int size) {
    int result;
    __asm__ volatile (
        "int $0x24"
        : "=a"(result)
        : "D"(path), "S"(buffer), "d"((uint64_t)size)
        : "memory"
    );
    return result;
}

static inline int write(const char *path, const void *buffer, int size) {
    int result;
    __asm__ volatile (
        "int $0x25"
        : "=a"(result)
        : "D"(path), "S"(buffer), "d"((uint64_t)size)
        : "memory"
    );
    return result;
}

static inline int filelength(const char *path) {
    int result;
    __asm__ volatile (
        "int $0x27"
        : "=a"(result)
        : "D"(path)
        : "memory"
    );
    return result;
}

static inline int writeb(const char *path, uint64_t byte) {
    int result;
    __asm__ volatile (
        "int $0x26"
        : "=a"(result)
        : "D"(path), "S"(byte)
        : "memory"
    );
    return result;
}

static inline void flushstdout() {
    __asm__ volatile ("int $0x30");
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

#endif
