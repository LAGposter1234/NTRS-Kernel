#pragma once
#include <stdint.h>
#include <stddef.h>

#define FAT16_SECTOR_SIZE 512
#define FAT16_MAX_FATS 2
#define FAT16_ATTR_READ_ONLY 0x01
#define FAT16_ATTR_HIDDEN 0x02
#define FAT16_ATTR_SYSTEM 0x04
#define FAT16_ATTR_VOLUME_ID 0x08
#define FAT16_ATTR_DIRECTORY 0x10
#define FAT16_ATTR_ARCHIVE 0x20
#define FAT16_EOC 0xFFF8

extern int ata_read_sector(uint8_t disk, uint32_t lba, void *buffer);
extern int ata_write_sector(uint8_t disk, uint32_t lba, const void *buffer);

struct fat16 {
    uint8_t disk;
    uint8_t mounted;
    uint8_t dirty;
    uint8_t fat_count;
    uint8_t sectors_per_cluster;
    uint16_t bytes_per_sector;
    uint16_t root_entries;
    uint16_t sectors_per_fat;
    uint32_t partition_start;
    uint32_t total_sectors;
    uint32_t fat_start;
    uint32_t root_start;
    uint32_t root_sectors;
    uint32_t data_start;
    uint32_t cluster_count;
    uint32_t fat_bytes;
    uint8_t *fat;
    uint32_t fat_capacity;
    uint8_t *sector_buffer;
    uint8_t *cluster_buffer;
    uint32_t cluster_buffer_size;
};

struct fat16_file {
    uint32_t size;
    uint16_t first_cluster;
    uint8_t attr;
};

static uint16_t fat16_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t fat16_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void fat16_put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

void fat16_put32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

int fat16_read(struct fat16 *fs, uint32_t lba, void *buf) {
    if (lba >= fs->partition_start + fs->total_sectors)
        return -1;
    return ata_read_sector(fs->disk, lba, buf);
}

int fat16_write_sector(struct fat16 *fs, uint32_t lba, const void *buf) {
    if (lba >= fs->partition_start + fs->total_sectors)
        return -1;
    if (ata_write_sector(fs->disk, lba, buf) != 0)
        return -1;
    return 0;
}

int fat16_read_cluster(struct fat16 *fs, uint16_t cluster, uint8_t *buf) {
    uint32_t lba;
    uint32_t i;
    if (cluster < 2 || cluster >= fs->cluster_count + 2)
        return -1;
    lba = fs->data_start + ((uint32_t)cluster - 2) * fs->sectors_per_cluster;
    for (i = 0; i < fs->sectors_per_cluster; i++)
        if (fat16_read(fs, lba + i, buf + i * FAT16_SECTOR_SIZE) != 0)
            return -1;
    return 0;
}

int fat16_write_cluster(struct fat16 *fs, uint16_t cluster, const uint8_t *buf) {
    uint32_t lba;
    uint32_t i;
    if (cluster < 2 || cluster >= fs->cluster_count + 2)
        return -1;
    lba = fs->data_start + ((uint32_t)cluster - 2) * fs->sectors_per_cluster;
    for (i = 0; i < fs->sectors_per_cluster; i++)
        if (fat16_write_sector(fs, lba + i, buf + i * FAT16_SECTOR_SIZE) != 0)
            return -1;
    return 0;
}

static uint16_t fat16_next(struct fat16 *fs, uint16_t cluster) {
    if (cluster < 2 || (uint32_t)cluster * 2 + 1 >= fs->fat_bytes)
        return 0;
    return fat16_u16(fs->fat + (uint32_t)cluster * 2);
}

int fat16_set_next(struct fat16 *fs, uint16_t cluster, uint16_t next) {
    if (cluster < 2 || (uint32_t)cluster * 2 + 1 >= fs->fat_bytes)
        return -1;
    fat16_put16(fs->fat + (uint32_t)cluster * 2, next);
    fs->dirty = 1;
    return 0;
}

int fat16_is_eoc(uint16_t c) {
    return c >= FAT16_EOC;
}

int fat16_name_equal(const uint8_t *entry, const char *name, uint32_t len) {
    uint32_t i, base_len = 0, ext_len = 0, dot = len;
    while (base_len < 8 && entry[base_len] != ' ')
        base_len++;
    while (ext_len < 3 && entry[8 + ext_len] != ' ')
        ext_len++;
    for (i = 0; i < len; i++)
        if (name[i] == '.') { dot = i; break; }
    if (dot != base_len)
        return 0;
    for (i = 0; i < base_len; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((uint8_t)c != entry[i]) return 0;
    }
    if (dot == len)
        return ext_len == 0;
    if (len - dot - 1 != ext_len)
        return 0;
    for (i = 0; i < ext_len; i++) {
        char c = name[dot + 1 + i];
        if (c >= 'a' && c <= 'z') c -= 32;
        if ((uint8_t)c != entry[8 + i]) return 0;
    }
    return 1;
}

int fat16_find_in_dir(struct fat16 *fs, uint16_t dir_cluster,
                             int root, const char *name, uint32_t name_len,
                             uint8_t out[32]) {
    uint32_t i, j, lba, sectors;
    uint8_t *buf = fs->cluster_buffer;
    if (root) {
        lba = fs->root_start;
        sectors = fs->root_sectors;
        for (i = 0; i < sectors; i++) {
            if (fat16_read(fs, lba + i, fs->sector_buffer) != 0) return -1;
            for (j = 0; j < FAT16_SECTOR_SIZE; j += 32) {
                uint8_t *e = fs->sector_buffer + j;
                if (e[0] == 0x00) return 1;
                if (e[0] == 0xE5 || e[11] == 0x0F || (e[11] & FAT16_ATTR_VOLUME_ID)) continue;
                if (fat16_name_equal(e, name, name_len)) {
                    for (j = 0; j < 32; j++) out[j] = e[j];
                    return 0;
                }
            }
        }
        return 1;
    }
    {
        uint16_t c = dir_cluster;
        uint32_t steps = 0;
        while (c >= 2 && c < fs->cluster_count + 2 && steps++ <= fs->cluster_count) {
            if (fat16_read_cluster(fs, c, buf) != 0) return -1;
            for (i = 0; i < fs->cluster_buffer_size; i += 32) {
                uint8_t *e = buf + i;
                if (e[0] == 0x00) return 1;
                if (e[0] == 0xE5 || e[11] == 0x0F || (e[11] & FAT16_ATTR_VOLUME_ID)) continue;
                if (fat16_name_equal(e, name, name_len)) {
                    for (j = 0; j < 32; j++) out[j] = e[j];
                    return 0;
                }
            }
            c = fat16_next(fs, c);
            if (fat16_is_eoc(c) || c == 0) break;
        }
    }
    return 1;
}

int fat16_mount(struct fat16 *fs, uint8_t disk, uint8_t *fat_buffer,
                uint32_t fat_capacity, uint8_t *sector_buffer,
                uint8_t *cluster_buffer, uint32_t cluster_buffer_size) {
    uint8_t boot[FAT16_SECTOR_SIZE];
    uint32_t start = 0, i, fat_bytes, root_sectors, total;
    uint16_t reserved, root_entries, spf;
    uint8_t spc, fats;

    if (!fs || !fat_buffer || !sector_buffer || !cluster_buffer)
        return -1;

    if (ata_read_sector(disk, 0, boot) != 0)
        return -2;

    if (boot[510] != 0x55 || boot[511] != 0xAA)
        return -3;

    if (boot[13] == 0 || boot[16] == 0 || fat16_u16(boot + 22) == 0) {
        int found = 0;

        for (i = 0; i < 4; i++) {
            uint8_t *p = boot + 446 + i * 16;
            uint8_t type = p[4];
            uint32_t pstart = fat16_u32(p + 8);

            if (type == 0x04 || type == 0x06 || type == 0x0E) {
                start = pstart;
                found = 1;
                break;
            }
        }

        if (!found || !start || ata_read_sector(disk, start, boot) != 0)
            return -4;

        if (boot[510] != 0x55 || boot[511] != 0xAA)
            return -5;
    }

    if (fat16_u16(boot + 11) != FAT16_SECTOR_SIZE)
        return -6;

    spc = boot[13];
    reserved = fat16_u16(boot + 14);
    fats = boot[16];
    root_entries = fat16_u16(boot + 17);
    total = fat16_u16(boot + 19);
    if (!total)
        total = fat16_u32(boot + 32);
    spf = fat16_u16(boot + 22);

    if (!spc || (spc & (spc - 1)) || !reserved || !fats ||
        fats > FAT16_MAX_FATS || !root_entries || !spf || !total)
        return -7;

    root_sectors = ((uint32_t)root_entries * 32 + 511) / 512;

    if ((uint32_t)reserved + (uint32_t)fats * spf + root_sectors >= total)
        return -8;

    fat_bytes = (uint32_t)spf * 512;

    if (fat_bytes > fat_capacity ||
        (uint32_t)spc * 512 > cluster_buffer_size)
        return -9;

    fs->disk = disk;
    fs->mounted = 0;
    fs->dirty = 0;
    fs->partition_start = start;
    fs->bytes_per_sector = FAT16_SECTOR_SIZE;
    fs->sectors_per_cluster = spc;
    fs->fat_count = fats;
    fs->root_entries = root_entries;
    fs->sectors_per_fat = spf;
    fs->total_sectors = total;
    fs->fat_start = start + reserved;
    fs->root_start = fs->fat_start + (uint32_t)fats * spf;
    fs->root_sectors = root_sectors;
    fs->data_start = fs->root_start + root_sectors;
    fs->cluster_count =
        (total - (reserved + (uint32_t)fats * spf + root_sectors)) / spc;
    fs->fat_bytes = fat_bytes;
    fs->fat = fat_buffer;
    fs->fat_capacity = fat_capacity;
    fs->sector_buffer = sector_buffer;
    fs->cluster_buffer = cluster_buffer;
    fs->cluster_buffer_size = cluster_buffer_size;

    if (fs->cluster_count < 4085 || fs->cluster_count >= 65525)
        return -10;

    for (i = 0; i < spf; i++) {
        if (ata_read_sector(disk, fs->fat_start + i, sector_buffer) != 0)
            return -11;

        for (uint32_t j = 0; j < FAT16_SECTOR_SIZE; j++)
            fat_buffer[i * FAT16_SECTOR_SIZE + j] = sector_buffer[j];
    }

    fs->mounted = 1;
    return 0;
}

int fat16_lookup(struct fat16 *fs, const char *path, struct fat16_file *file) {
    uint16_t dir_cluster = 0;
    int root = 1;
    uint8_t entry[32];
    const char *p = path;
    if (!fs || !fs->mounted || !path || !file) return -1;
    while (*p == '/') p++;
    if (!*p) return -1;
    for (;;) {
        const char *start = p;
        uint32_t len;
        while (*p && *p != '/') p++;
        len = (uint32_t)(p - start);
        while (*p == '/') p++;
        if (!len || len > 12) return -1;
        {
            int r = fat16_find_in_dir(fs, dir_cluster, root, start, len, entry);
            if (r != 0) return -1;
        }
        if (*p) {
            if (!(entry[11] & FAT16_ATTR_DIRECTORY)) return -1;
            dir_cluster = fat16_u16(entry + 26);
            if (dir_cluster < 2) return -1;
            root = 0;
        } else {
            file->attr = entry[11];
            file->first_cluster = fat16_u16(entry + 26);
            file->size = fat16_u32(entry + 28);
            return 0;
        }
    }
}

int fat16_read_file(struct fat16 *fs, const char *path, void *buffer, uint32_t capacity, uint32_t *bytes_read) {
    struct fat16_file file;
    uint8_t *out = (uint8_t *)buffer;
    uint32_t remaining, copied = 0, i;
    uint16_t cluster;
    if (!buffer || fat16_lookup(fs, path, &file) != 0) return -1;
    if (file.attr & FAT16_ATTR_DIRECTORY) return -1;
    if (capacity < file.size) return -1;
    remaining = file.size;
    cluster = file.first_cluster;
    while (remaining) {
        uint32_t n = remaining < fs->cluster_buffer_size ? remaining : fs->cluster_buffer_size;
        if (fat16_read_cluster(fs, cluster, fs->cluster_buffer) != 0) return -1;
        for (i = 0; i < n; i++) out[copied + i] = fs->cluster_buffer[i];
        copied += n;
        remaining -= n;
        if (remaining) {
            uint16_t next = fat16_next(fs, cluster);
            if (fat16_is_eoc(next) || next < 2) return -1;
            cluster = next;
        }
    }
    if (bytes_read) *bytes_read = copied;
    return 0;
}

static int fat16_find_entry_location(struct fat16 *fs, const char *path,
                                     uint32_t *lba, uint32_t *offset,
                                     uint8_t entry[32]) {
    uint16_t dir = 0;
    int root = 1;
    const char *p = path;
    while (*p == '/') p++;
    if (!*p) return -1;
    for (;;) {
        const char *name = p;
        uint32_t len, i, j;
        while (*p && *p != '/') p++;
        len = (uint32_t)(p - name);
        while (*p == '/') p++;
        if (!len || len > 12) return -1;
        if (*p) {
            if (fat16_find_in_dir(fs, dir, root, name, len, entry) != 0)
                return -1;
            if (!(entry[11] & FAT16_ATTR_DIRECTORY)) return -1;
            dir = fat16_u16(entry + 26);
            if (dir < 2 || dir >= fs->cluster_count + 2) return -1;
            root = 0;
            continue;
        }
        if (root) {
            for (i = 0; i < fs->root_sectors; i++) {
                if (fat16_read(fs, fs->root_start + i,
                               fs->sector_buffer) != 0) return -1;
                for (j = 0; j < 512; j += 32) {
                    uint8_t *e = fs->sector_buffer + j;
                    if (e[0] == 0) return -1;
                    if (e[0] == 0xE5 || e[11] == 0x0F ||
                        (e[11] & FAT16_ATTR_VOLUME_ID)) continue;
                    if (fat16_name_equal(e, name, len)) {
                        *lba = fs->root_start + i;
                        *offset = j;
                        for (uint32_t k = 0; k < 32; k++)
                            entry[k] = e[k];
                        return 0;
                    }
                }
            }
            return -1;
        }
        {
            uint16_t c = dir;
            uint32_t steps = 0;
            while (c >= 2 && c < fs->cluster_count + 2 &&
                   steps++ < fs->cluster_count) {
                uint32_t base = fs->data_start +
                    ((uint32_t)c - 2) * fs->sectors_per_cluster;
                for (i = 0; i < fs->sectors_per_cluster; i++) {
                    if (fat16_read(fs, base + i, fs->sector_buffer) != 0)
                        return -1;
                    for (j = 0; j < 512; j += 32) {
                        uint8_t *e = fs->sector_buffer + j;
                        if (e[0] == 0) return -1;
                        if (e[0] == 0xE5 || e[11] == 0x0F ||
                            (e[11] & FAT16_ATTR_VOLUME_ID)) continue;
                        if (fat16_name_equal(e, name, len)) {
                            *lba = base + i;
                            *offset = j;
                            for (uint32_t k = 0; k < 32; k++)
                                entry[k] = e[k];
                            return 0;
                        }
                    }
                }
                c = fat16_next(fs, c);
                if (fat16_is_eoc(c) || c < 2) break;
            }
        }
        return -1;
    }
}

int fat16_sync_to_disk(struct fat16 *fs) {
    uint32_t copy, sector, i;
    if (!fs || !fs->mounted) return -1;
    if (!fs->dirty) return 0;
    for (copy = 0; copy < fs->fat_count; copy++) {
        for (sector = 0; sector < fs->sectors_per_fat; sector++) {
            for (i = 0; i < 512; i++)
                fs->sector_buffer[i] = fs->fat[sector * 512 + i];
            if (fat16_write_sector(fs,
                fs->fat_start + copy * fs->sectors_per_fat + sector,
                fs->sector_buffer) != 0)
                return -1;
        }
    }
    fs->dirty = 0;
    return 0;
}

int fat16_write_file(struct fat16 *fs, const char *path, const void *buffer, uint32_t size) {
    struct fat16_file file;
    uint8_t entry[32];
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t dir_lba, dir_offset, cluster_bytes, needed, i, copied = 0;
    uint16_t current, previous = 0, first = 0;
    if (!fs || !fs->mounted || !path || (!buffer && size))
        return -1;
    cluster_bytes = (uint32_t)fs->sectors_per_cluster * 512;
    if (!cluster_bytes || cluster_bytes > fs->cluster_buffer_size)
        return -1;
    if (fat16_lookup(fs, path, &file) != 0) return -1;
    if (file.attr & (FAT16_ATTR_DIRECTORY | FAT16_ATTR_READ_ONLY))
        return -1;
    if (fat16_find_entry_location(fs, path, &dir_lba,
                                  &dir_offset, entry) != 0)
        return -1;
    needed = size / cluster_bytes + (size % cluster_bytes != 0);
    if (needed > fs->cluster_count) return -1;
    current = file.first_cluster;
    for (i = 0; i < needed; i++) {
        uint16_t use, next = 0;
        if (current >= 2 && current < fs->cluster_count + 2) {
            use = current;
            next = fat16_next(fs, current);
            if (fat16_is_eoc(next) || next == 0)
                next = 0;
            else if (next < 2 || next >= fs->cluster_count + 2)
                return -1;
        } else {
            uint32_t c;
            use = 0;
            for (c = 2; c < fs->cluster_count + 2; c++) {
                if (fat16_next(fs, (uint16_t)c) == 0) {
                    use = (uint16_t)c;
                    break;
                }
            }
            if (!use) return -1;
        }
        if (!first) first = use;
        if (previous && fat16_set_next(fs, previous, use) != 0)
            return -1;
        if (fat16_set_next(fs, use, 0xFFFF) != 0)
            return -1;
        for (uint32_t j = 0; j < cluster_bytes; j++)
            fs->cluster_buffer[j] =
                j < size - copied ? src[copied + j] : 0;
        if (fat16_write_cluster(fs, use, fs->cluster_buffer) != 0)
            return -1;
        copied += size - copied < cluster_bytes ?
                  size - copied : cluster_bytes;
        previous = use;
        current = next;
    }
    if (previous && fat16_set_next(fs, previous, 0xFFFF) != 0)
        return -1;
    for (i = 0; current >= 2 &&
         current < fs->cluster_count + 2 && i < fs->cluster_count; i++) {
        uint16_t next = fat16_next(fs, current);
        if (fat16_set_next(fs, current, 0) != 0) return -1;
        if (fat16_is_eoc(next) || next == 0) {
            current = 0;
            break;
        }
        if (next < 2 || next >= fs->cluster_count + 2) return -1;
        current = next;
    }
    if (current >= 2 && current < fs->cluster_count + 2)
        return -1;
    if (fat16_sync_to_disk(fs) != 0) return -1;
    if (fat16_read(fs, dir_lba, fs->sector_buffer) != 0) return -1;
    fat16_put16(fs->sector_buffer + dir_offset + 26, first);
    fat16_put32(fs->sector_buffer + dir_offset + 28, size);
    if (fat16_write_sector(fs, dir_lba, fs->sector_buffer) != 0)
        return -1;
    return 0;
}


static int fat16_check_volume(uint8_t disk, uint32_t start) {
    uint8_t boot[512];
    uint32_t total, root_sectors, clusters;
    uint16_t reserved, roots, spf;
    uint8_t spc, fats;

    if (ata_read_sector(disk, start, boot) != 0)
        return 0;
    if (boot[510] != 0x55 || boot[511] != 0xAA)
        return 0;
    if (fat16_u16(boot + 11) != 512)
        return 0;

    spc = boot[13];
    reserved = fat16_u16(boot + 14);
    fats = boot[16];
    roots = fat16_u16(boot + 17);
    total = fat16_u16(boot + 19);
    if (!total) total = fat16_u32(boot + 32);
    spf = fat16_u16(boot + 22);

    if (!spc || (spc & (spc - 1)) || !reserved ||
        !fats || fats > 2 || !roots || !spf || !total)
        return 0;

    root_sectors = ((uint32_t)roots * 32 + 511) / 512;
    if ((uint32_t)reserved + (uint32_t)fats * spf +
        root_sectors >= total)
        return 0;

    clusters = (total - reserved - (uint32_t)fats * spf -
    root_sectors) / spc;

    return clusters >= 4085 && clusters < 65525;
}

int globaldatadrive = -1;
#define FAT_RAM_BASE 0x0FF00000
#define FAT_RAM_SIZE 0x00100000
#define FAT_CAPACITY 65536
#define CLUSTER_CAPACITY 4096

static struct fat16 datafs;
static uint8_t *data_fat = (uint8_t *)(FAT_RAM_BASE);
static uint8_t *data_sector = (uint8_t *)(FAT_RAM_BASE + 0x10000);
static uint8_t *data_cluster = (uint8_t *)(FAT_RAM_BASE + 0x11000);

int init_data_drive(void) {
    if (globaldatadrive < 0)
        return -1;

    return fat16_mount(&datafs, globaldatadrive,
                       data_fat, FAT_CAPACITY,
                       data_sector, data_cluster, CLUSTER_CAPACITY);
}
