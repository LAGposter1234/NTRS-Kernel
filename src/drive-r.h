#pragma once
#include <stdint.h>

#define ATA_DATA       0x1F0
#define ATA_ERROR      0x1F1
#define ATA_SECCOUNT   0x1F2
#define ATA_LBA0       0x1F3
#define ATA_LBA1       0x1F4
#define ATA_LBA2       0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_STATUS     0x1F7
#define ATA_COMMAND    0x1F7

#define ATA_SR_ERR     0x01
#define ATA_SR_DRQ     0x08
#define ATA_SR_DF      0x20
#define ATA_SR_BSY     0x80

#define ATA_CMD_READ   0x20
#define ATA_TIMEOUT    1000000

static int ata_wait(uint8_t set, uint8_t clear) {
    for (uint32_t i = 0; i < ATA_TIMEOUT; i++) {
        uint8_t status = inb(ATA_STATUS);
        if (status == 0x00 || status == 0xFF || (status & (ATA_SR_ERR | ATA_SR_DF))) {
            return -1;
        }
        if ((status & set) == set && !(status & clear)) {
            return 0;
        }
    }
    return -1;
}

int ata_read_sector(uint8_t drive, uint32_t lba, void *buffer) {
    if (!buffer || lba >= 0x10000000) {
        return -1;
    }

    outb(ATA_DRIVE, 0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F));
    for (int i = 0; i < 4; i++) {
        inb(ATA_STATUS);
    }

    if (ata_wait(0, ATA_SR_BSY) != 0) {
        return -1;
    }

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA0, (uint8_t)lba);
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_READ);

    if (ata_wait(ATA_SR_DRQ, ATA_SR_BSY) != 0) {
        return -1;
    }

    uint16_t *dst = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        dst[i] = inw(ATA_DATA);
    }

    return 0;
}

#define ATA_CMD_WRITE 0x30

int ata_write_sector(uint8_t drive, uint32_t lba, const void *buffer) {
    if (!buffer || lba >= 0x10000000) {
        return -1;
    }

    outb(ATA_DRIVE, 0xE0 | ((drive & 1) << 4) | ((lba >> 24) & 0x0F));
    for (int i = 0; i < 4; i++) {
        inb(ATA_STATUS);
    }

    if (ata_wait(0, ATA_SR_BSY) != 0) {
        return -1;
    }

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA0, (uint8_t)lba);
    outb(ATA_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_LBA2, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    if (ata_wait(ATA_SR_DRQ, ATA_SR_BSY) != 0) {
        return -1;
    }

    const uint16_t *src = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        __asm__ volatile ("outw %0, %1" :: "a"(src[i]), "Nd"((uint16_t)ATA_DATA));
    }

    if (ata_wait(0, ATA_SR_BSY) != 0) {
        return -1;
    }

    outb(ATA_COMMAND, 0xE7);
    return ata_wait(0, ATA_SR_BSY);
}
