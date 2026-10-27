#include <stdint.h>
#include <stddef.h>
#include "io.h"
#define MAX_ARGS 16
#include "smallos.h"
#include "config.h"
#include "interrupts.h"
#include "drive-r.h"
#include "ps2.h"
#include "serial.h"
#include "font.h"
#include "graphics.h"
#include "fat16.h"

#ifdef SOUNDSUPPORT
#include "sb16.h"

#define SB16_BUFFER_PHYS 0x80000
#define SB16_BUFFER_SIZE 4096

static uint8_t *sb16_buffer = (uint8_t *)SB16_BUFFER_PHYS;

static void sb16_make_kick(void) {
    uint32_t phase = 0;

    for (uint32_t i = 0; i < SB16_BUFFER_SIZE; i++) {
        uint32_t freq = 45 + 105 * (SB16_BUFFER_SIZE - i) / SB16_BUFFER_SIZE;
        phase += (freq * 65536u) / 11025u;

        int32_t wave = (phase & 0x8000u) ? 64 : -64;
        int32_t envelope = (int32_t)(SB16_BUFFER_SIZE - i);
        int32_t sample = wave * envelope / (int32_t)SB16_BUFFER_SIZE;

        sb16_buffer[i] = (uint8_t)(128 + sample);
    }
}

#endif

uint8_t terminal[105][80];
uint8_t terminal_colours[105][80];
uint8_t cx, cy;

void term_render() {
    clear(0);
    for (int y = 0; y < 105; y++) {
        for (int x = 0; x < 80; x++) {
            if (terminal[y][x] != ' ')
                draw_char(terminal[y][x], terminal_colours[y][x], x * 6, y * 6);
        }
    }
    flush();
}

void term_print(char *s, uint8_t color) {
    while (*s) {
        if (*s == '\n') {
            cx = 0;
            cy++;
            s++;
        } else {
            terminal[cy][cx] = *s++;
            terminal_colours[cy][cx] = color;
            cx++;
        }
        if (cx >= 80) {
            cx = 0;
            cy++;
        }
        if (cy >= 105) {
            for (uint8_t y = 1; y < 105; y++) {
                for (uint8_t x = 0; x < 80; x++) {
                    terminal[y - 1][x] = terminal[y][x];
                    terminal_colours[y - 1][x] = terminal_colours[y][x];
                }
            }
            for (uint8_t x = 0; x < 80; x++) {
                terminal[104][x] = ' ';
                terminal_colours[104][x] = 0;
            }
            cy = 104;
        }
    }
    term_render();
}

void term_print_noflush(char *s, uint8_t color) {
    while (*s) {
        if (*s == '\n') {
            cx = 0;
            cy++;
            s++;
        } else {
            terminal[cy][cx] = *s++;
            terminal_colours[cy][cx] = color;
            cx++;
        }
        if (cx >= 80) {
            cx = 0;
            cy++;
        }
        if (cy >= 105) {
            for (uint8_t y = 1; y < 105; y++) {
                for (uint8_t x = 0; x < 80; x++) {
                    terminal[y - 1][x] = terminal[y][x];
                    terminal_colours[y - 1][x] = terminal_colours[y][x];
                }
            }
            for (uint8_t x = 0; x < 80; x++) {
                terminal[104][x] = ' ';
                terminal_colours[104][x] = 0;
            }
            cy = 104;
        }
    }
    term_render();
}

void terminal_init() {
    cx = 0;
    cy = 0;
    for (uint8_t y = 0; y < 105; y++) {
        for (uint8_t x = 0; x < 80; x++) {
            terminal[y][x] = ' ';
            terminal_colours[y][x] = 0;
        }
    }
    term_render();
}

char* itoa(int value, char* str, int base) {
    int i = 0;
    int isNegative = 0;
    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (value < 0 && base == 10) {
        isNegative = 1;
        value = -value;
    }

    while (value != 0) {
        int remainder = value % base;
        str[i++] = (remainder > 9) ? (remainder - 10) + 'a' : remainder + '0';
        value /= base;
    }

    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = '\0';

    for (int j = 0; j < i / 2; j++) {
        char temp = str[j];
        str[j] = str[i - j - 1];
        str[i - j - 1] = temp;
    }

    return str;
}

#define FAT16_EXEC_FAIL (-1610612735)

char shellprocessfn[64];
int fat16_tryexec(char *filename) {
    uint32_t bytes_read;
    int (*entry)(void);
    if (!filename)return FAT16_EXEC_FAIL;
    if (fat16_read_file(&datafs, filename, (void *)0x08000000, 0x07F00000, &bytes_read) != 0)return -1;
    if (!bytes_read)return FAT16_EXEC_FAIL;
    if (shellprocessfn[0] == 0) {
        strcpy(shellprocessfn, filename);
    }
    term_print("Executing ", 0xF);
    term_print(shellprocessfn, 0x2);
    term_print(" as shell process\n", 0xF);
    entry = (int (*)(void))0x08000000;
    return entry();
}

void try_execinit(void) {
    term_print("try exec init\n", 0xF);
    if (fat16_tryexec("/bin/sh") == FAT16_EXEC_FAIL) {
        term_print("dude where the fuck is the shell\n", 0xF);
    }
}

struct interrupt_frame {
    uintptr_t rip;
    uintptr_t cs;
    uintptr_t rflags;
    uintptr_t rsp;
    uintptr_t ss;
};

__attribute__((interrupt))
void interrupt_print(struct interrupt_frame *frame) {
    uint64_t rax;
    __asm__ volatile ("movq %%rdi, %0" : "=D"(rax));
    term_print((char*)rax, 0xF);
    (void)frame;
}

__attribute__((interrupt))
void interrupt_getln(struct interrupt_frame *frame) {
    char *buf;
    uint64_t size;
    uint64_t len = 0;

    __asm__ volatile ("mov %%rdi, %0" : "=r"(buf));
    __asm__ volatile ("mov %%rsi, %0" : "=r"(size));

    if (!buf || size == 0) return;

    while (len < size - 1) {
        uint16_t key = get_key();

        if (key == '\n') {
            term_print("\n", 0xF);
            break;
        }

        if (key == '\b') {
            if (len) {
                len--;
                term_print("\b \b", 0xF);
            }
            continue;
        }

        if (key >= 32 && key <= 126) {
            buf[len++] = (char)key;
            char s[2] = {(char)key, '\0'};
            term_print(s, 0xF);
        }
    }

    buf[len] = '\0';
    (void)frame;
}

__attribute__((interrupt))
void interrupt_read(struct interrupt_frame *frame) {
    uint64_t path, buffer, size;
    uint32_t bytes_read;
    __asm__ volatile ("movq %%rdi, %0" : "=r"(path));
    __asm__ volatile ("movq %%rsi, %0" : "=r"(buffer));
    __asm__ volatile ("movq %%rdx, %0" : "=r"(size));
    if (!path || !buffer || size > UINT32_MAX || fat16_read_file(&datafs, (const char *)path, (void *)buffer, (uint32_t)size, &bytes_read) != 0) {
        __asm__ volatile ("movq $-1, %%rax" ::: "rax");
        (void)frame;
        return;
    }
    __asm__ volatile ("movq %0, %%rax" : : "r"((uint64_t)bytes_read) : "rax");
    (void)frame;
}

__attribute__((interrupt))
void interrupt_write(struct interrupt_frame *frame) {
    uint64_t path, buffer, size;
    int result;
    __asm__ volatile ("movq %%rdi, %0" : "=r"(path));
    __asm__ volatile ("movq %%rsi, %0" : "=r"(buffer));
    __asm__ volatile ("movq %%rdx, %0" : "=r"(size));
    if (!path || (!buffer && size) || size > UINT32_MAX) {
        __asm__ volatile ("movq $-1, %%rax" ::: "rax");
        (void)frame;
        return;
    }
    if (strcmp((char*)path, "stdout") == 0) {
        char* string = (char*)buffer;
        string[size] = '\0';
        term_print(string, 0xF);
        (void)frame;
        return;
    }
    result = fat16_write_file(&datafs, (const char *)path, (const void *)buffer, (uint32_t)size);
    if (result != 0) {
        __asm__ volatile ("movq $-1, %%rax" ::: "rax");
        (void)frame;
        return;
    }
    __asm__ volatile ("movq %0, %%rax" : : "r"(size) : "rax");
    (void)frame;
}
__attribute__((interrupt))
void interrupt_writeb(struct interrupt_frame *frame) {
    uint64_t path, byte;
    int result;
    __asm__ volatile ("movq %%rdi, %0" : "=r"(path));
    __asm__ volatile ("movq %%rsi, %0" : "=r"(byte));
    uint8_t buffer[2];
    buffer[0] = (uint8_t)byte;
    buffer[1] = '\0';
    if (!path) {
        term_print_noflush(buffer, 0xF);
        (void)frame;
        return;
    }
    result = fat16_write_file(&datafs, (const char *)path, (const void *)buffer, 1);
    if (result != 0) {
        __asm__ volatile ("movq $-1, %%rax" ::: "rax");
        (void)frame;
        return;
    }
    (void)frame;
}

__attribute__((interrupt))
void interrupt_flushterm(struct interrupt_frame *frame) {
    term_render();
    (void)frame;
}

void register_interrupts() {
    register_interrupt_kernel(0x22, (void*)interrupt_print);
    register_interrupt_kernel(0x23, (void *)interrupt_getln);
    register_interrupt_kernel(0x24, (void*)interrupt_write);
    register_interrupt_kernel(0x25, (void*)interrupt_read);
    register_interrupt_kernel(0x26, (void*)interrupt_writeb);
    register_interrupt_kernel(0x30, (void*)interrupt_flushterm);
    lidt();
}

void init_all() {
    register_interrupts();
    ps2_mouse_init();;
#ifdef SOUNDSUPPORT
    sb16_init();
#endif
    vga_init();
    terminal_init();
}

static uint8_t bootdriveglobal = 0;

int fat16_find_drive(void) {
    uint8_t sector[512];
    int disk, i;

    globaldatadrive = -1;

    for (disk = 0; disk < 4; disk++) {
        if (fat16_check_volume(disk, 0)) {
            globaldatadrive = disk;
            return disk;
        }

        if (ata_read_sector(disk, 0, sector) != 0)
            continue;
        if (sector[510] != 0x55 || sector[511] != 0xAA)
            continue;

        for (i = 0; i < 4; i++) {
            uint8_t *p = sector + 446 + i * 16;
            uint8_t type = p[4];
            uint32_t start = fat16_u32(p + 8);

            if (type != 0x04 && type != 0x06 && type != 0x0E)
                continue;

            if (fat16_check_volume(disk, start)) {
                globaldatadrive = disk;
                return disk;
            }
        }
    }

    return -1;
}

int fat16_list_root(struct fat16 *fs) {
    uint32_t i, j;

    if (!fs || !fs->mounted)
        return -1;

    for (i = 0; i < fs->root_sectors; i++) {
        if (fat16_read(fs, fs->root_start + i, fs->sector_buffer) != 0)
            return -1;

        for (j = 0; j < 512; j += 32) {
            uint8_t *e = fs->sector_buffer + j;
            char name[13];
            uint32_t n = 0, k;

            if (e[0] == 0x00)
                return 0;
            if (e[0] == 0xE5 || e[11] == 0x0F ||
                (e[11] & FAT16_ATTR_VOLUME_ID))
                continue;

            for (k = 0; k < 8 && e[k] != ' '; k++) {
                char c = e[k];
                name[n++] = (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
            }

            if (e[8] != ' ') {
                name[n++] = '.';
                for (k = 8; k < 11 && e[k] != ' '; k++)
                    name[n++] = e[k];
            }

            name[n] = '\0';
            term_print(name, 0xF);
            term_print(e[11] & FAT16_ATTR_DIRECTORY ? "/\n" : "\n", 0xF);
        }
    }

    return 0;
}

int fat16_list_path(struct fat16 *fs, const char *path) {
    uint8_t entry[32], *e;
    const char *p;
    char name[13];
    uint32_t i, j, n, k, size;
    uint16_t cluster = 0;
    int root = 1;

    if (!fs || !fs->mounted || !path)
        return -1;

    p = path;
    while (*p == '/')
        p++;

    while (*p) {
        n = 0;
        while (p[n] && p[n] != '/')
            n++;

        if (!n) {
            p++;
            continue;
        }

        if (n > 12)
            return -1;

        for (i = 0; i < n; i++) {
            char c = p[i];
            name[i] = (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
        }
        name[n] = '\0';

        if (fat16_find_in_dir(fs, cluster, root, name, n, entry) != 0)
            return -1;

        if (!(entry[11] & FAT16_ATTR_DIRECTORY))
            return -1;

        cluster = (uint16_t)(entry[26] | ((uint16_t)entry[27] << 8));
        if (cluster < 2)
            return -1;

        root = 0;
        p += n;
        while (*p == '/')
            p++;
    }

    if (root) {
        for (i = 0; i < fs->root_sectors; i++) {
            if (fat16_read(fs, fs->root_start + i, fs->sector_buffer) != 0)
                return -1;

            for (j = 0; j < 512; j += 32) {
                e = fs->sector_buffer + j;

                if (e[0] == 0x00)
                    return 0;
                if (e[0] == 0xE5 || e[11] == 0x0F ||
                    (e[11] & FAT16_ATTR_VOLUME_ID))
                    continue;

                n = 0;
                for (k = 0; k < 8 && e[k] != ' '; k++) {
                    char c = e[k];
                    name[n++] = (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
                }

                if (e[8] != ' ') {
                    name[n++] = '.';
                    for (k = 8; k < 11 && e[k] != ' '; k++) {
                        char c = e[k];
                        name[n++] = (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
                    }
                }

                name[n] = '\0';
                term_print(name, 0xF);
                term_print(e[11] & FAT16_ATTR_DIRECTORY ? "/\n" : "\n", 0xF);
            }
        }
        return 0;
    }

    size = fs->sectors_per_cluster * 512;
    if (size > fs->cluster_buffer_size)
        return -1;

    while (cluster >= 2 && cluster < 0xFFF8) {
        if (fat16_read_cluster(fs, cluster, fs->cluster_buffer) != 0)
            return -1;

        for (j = 0; j + 32 <= size; j += 32) {
            e = fs->cluster_buffer + j;

            if (e[0] == 0x00)
                return 0;
            if (e[0] == 0xE5 || e[11] == 0x0F ||
                (e[11] & FAT16_ATTR_VOLUME_ID))
                continue;

            n = 0;
            for (k = 0; k < 8 && e[k] != ' '; k++) {
                char c = e[k];
                name[n++] = (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
            }

            if (e[8] != ' ') {
                name[n++] = '.';
                for (k = 8; k < 11 && e[k] != ' '; k++) {
                    char c = e[k];
                    name[n++] = (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
                }
            }

            name[n] = '\0';
            term_print(name, 0xF);
            term_print(e[11] & FAT16_ATTR_DIRECTORY ? "/\n" : "\n", 0xF);
        }

        cluster = fat16_next(fs, cluster);
    }

    return 0;
}

__attribute__((section(".text.entry")))
void kernel_main(uint8_t boot_drive) {
    bootdriveglobal = boot_drive;
    serial_init();
    init_all();
    term_print(OS() "\n", 0xF);
    char drivestr[12];
    int drive = fat16_find_drive();
    itoa(drive, drivestr, 16);
    term_print("Data drive is ", 0xF);
    term_print(drivestr, 0x2);
    term_print("\n", 0xF);
    int mount_result = init_data_drive();
    if (mount_result != 0) {
        term_print("FAT16 mount failed\n", 0x4);
        char retstr[12];
        itoa(-mount_result, retstr, 16);
        term_print(retstr, 0xF);
        term_print("\n", 0xF);
    } else {
        term_print("FAT16 mounted\n", 0xF);
    }
    memset(shellprocessfn, 0, 32);
    try_execinit();
    for (;;);
}
