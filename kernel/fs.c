/* =============================================================================
 * SENG21213-OS :: RAM Disk File System (L12 Section 1, Stage 4)
 * File: kernel/fs.c
 *
 * Layout on the ramdisk (512-byte blocks):
 *   Block 0        : superblock
 *   Block 1        : directory (16 x 32-byte dirent_t entries)
 *   Blocks 2-5     : inode table (12 x 40-byte inode_t entries per block, 48 total)
 *   Block 6        : free-block bitmap (1 bit per block, 2048 bits = 256 bytes)
 *   Blocks 7-2047  : data blocks
 * All metadata genuinely lives on the ramdisk (read/written via rd_read /
 * rd_write on every operation) rather than being cached in kernel globals -
 * this mirrors how a real indexed/i-node file system works (L12 1.2).
 * ============================================================================*/
#include "fs.h"
#include "ramdisk.h"

#define SB_BLOCK            0u
#define DIR_BLOCK           1u
#define INODE_TABLE_START   2u
#define INODE_TABLE_BLOCKS  4u
#define BITMAP_BLOCK        6u
#define DATA_START_BLOCK    7u

#define INODES_PER_BLOCK  (FS_BLOCK_SIZE / sizeof(inode_t))
#define DIRENTS_PER_BLOCK (FS_BLOCK_SIZE / sizeof(dirent_t))

typedef struct {
    int      used;
    int      inode_num;
    uint32_t position;
} fs_file_t;

static fs_file_t open_files[FS_MAX_OPEN_FILES];

static int fs_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static void block_read(uint32_t block, uint8_t *buf)  { rd_read(block, buf); }
static void block_write(uint32_t block, uint8_t *buf) { rd_write(block, buf); }

static void sb_save(const superblock_t *sb) {
    uint8_t buf[FS_BLOCK_SIZE];
    for (uint32_t i = 0; i < FS_BLOCK_SIZE; i++) buf[i] = 0;
    for (uint32_t i = 0; i < sizeof(superblock_t); i++) buf[i] = ((const uint8_t *)sb)[i];
    block_write(SB_BLOCK, buf);
}

static void inode_load(uint32_t idx, inode_t *out) {
    uint32_t block  = INODE_TABLE_START + idx / INODES_PER_BLOCK;
    uint32_t offset = (idx % INODES_PER_BLOCK) * sizeof(inode_t);
    uint8_t buf[FS_BLOCK_SIZE];
    block_read(block, buf);
    for (uint32_t i = 0; i < sizeof(inode_t); i++) ((uint8_t *)out)[i] = buf[offset + i];
}

static void inode_save(uint32_t idx, const inode_t *in) {
    uint32_t block  = INODE_TABLE_START + idx / INODES_PER_BLOCK;
    uint32_t offset = (idx % INODES_PER_BLOCK) * sizeof(inode_t);
    uint8_t buf[FS_BLOCK_SIZE];
    block_read(block, buf);
    for (uint32_t i = 0; i < sizeof(inode_t); i++) buf[offset + i] = ((const uint8_t *)in)[i];
    block_write(block, buf);
}

static void dirent_load(uint32_t idx, dirent_t *out) {
    uint8_t buf[FS_BLOCK_SIZE];
    block_read(DIR_BLOCK, buf);
    uint32_t offset = idx * sizeof(dirent_t);
    for (uint32_t i = 0; i < sizeof(dirent_t); i++) ((uint8_t *)out)[i] = buf[offset + i];
}

static void dirent_save(uint32_t idx, const dirent_t *in) {
    uint8_t buf[FS_BLOCK_SIZE];
    block_read(DIR_BLOCK, buf);
    uint32_t offset = idx * sizeof(dirent_t);
    for (uint32_t i = 0; i < sizeof(dirent_t); i++) buf[offset + i] = ((const uint8_t *)in)[i];
    block_write(DIR_BLOCK, buf);
}

static int fs_bitmap_test(uint32_t block) {
    uint8_t buf[FS_BLOCK_SIZE];
    block_read(BITMAP_BLOCK, buf);
    return (buf[block / 8] >> (block % 8)) & 1;
}

static void fs_bitmap_set(uint32_t block, int val) {
    uint8_t buf[FS_BLOCK_SIZE];
    block_read(BITMAP_BLOCK, buf);
    if (val) buf[block / 8] |= (uint8_t)(1u << (block % 8));
    else     buf[block / 8] &= (uint8_t)~(1u << (block % 8));
    block_write(BITMAP_BLOCK, buf);
}

static uint32_t fs_bitmap_alloc(void) {
    for (uint32_t b = DATA_START_BLOCK; b < FS_TOTAL_BLOCKS; b++) {
        if (!fs_bitmap_test(b)) { fs_bitmap_set(b, 1); return b; }
    }
    return 0;
}

void fs_init(void) {
    uint8_t zero[FS_BLOCK_SIZE];
    for (uint32_t i = 0; i < FS_BLOCK_SIZE; i++) zero[i] = 0;

    block_write(DIR_BLOCK, zero);
    for (uint32_t b = 0; b < INODE_TABLE_BLOCKS; b++) block_write(INODE_TABLE_START + b, zero);
    block_write(BITMAP_BLOCK, zero);

    for (uint32_t b = 0; b < DATA_START_BLOCK; b++) fs_bitmap_set(b, 1);

    for (int fd = 0; fd < FS_MAX_OPEN_FILES; fd++) open_files[fd].used = 0;

    superblock_t sb;
    sb.magic               = FS_MAGIC;
    sb.block_count         = FS_TOTAL_BLOCKS;
    sb.inode_count         = FS_MAX_INODES;
    sb.free_bitmap_offset  = BITMAP_BLOCK;
    sb_save(&sb);
}

int fs_create(const char *name) {
    dirent_t d;
    for (uint32_t i = 0; i < FS_MAX_DIRENTS; i++) {
        dirent_load(i, &d);
        if (d.inode_num != 0 && fs_strcmp(d.name, name) == 0) return -1;
    }

    int free_dirent = -1;
    for (uint32_t i = 0; i < FS_MAX_DIRENTS; i++) {
        dirent_load(i, &d);
        if (d.inode_num == 0) { free_dirent = (int)i; break; }
    }
    if (free_dirent < 0) return -1;

    int free_inode = -1;
    inode_t ino;
    for (uint32_t i = 0; i < FS_MAX_INODES; i++) {
        inode_load(i, &ino);
        if (ino.type == 0) { free_inode = (int)i; break; }
    }
    if (free_inode < 0) return -1;

    ino.type = 1;
    ino.size = 0;
    for (int i = 0; i < FS_DIRECT_PTRS; i++) ino.direct[i] = 0;
    inode_save((uint32_t)free_inode, &ino);

    d.inode_num = (uint32_t)free_inode + 1;
    int i = 0;
    while (name[i] && i < FS_MAX_NAME - 1) { d.name[i] = name[i]; i++; }
    d.name[i] = '\0';
    dirent_save((uint32_t)free_dirent, &d);

    return 0;
}

int fs_open(const char *name) {
    dirent_t d;
    for (uint32_t i = 0; i < FS_MAX_DIRENTS; i++) {
        dirent_load(i, &d);
        if (d.inode_num != 0 && fs_strcmp(d.name, name) == 0) {
            for (int fd = 0; fd < FS_MAX_OPEN_FILES; fd++) {
                if (!open_files[fd].used) {
                    open_files[fd].used      = 1;
                    open_files[fd].inode_num = (int)(d.inode_num - 1);
                    open_files[fd].position  = 0;
                    return fd;
                }
            }
            return -1;
        }
    }
    return -1;
}

int fs_read(int fd, uint8_t *buf, uint32_t n) {
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !open_files[fd].used) return -1;

    inode_t ino;
    inode_load((uint32_t)open_files[fd].inode_num, &ino);

    uint32_t remaining = (ino.size > open_files[fd].position) ? (ino.size - open_files[fd].position) : 0;
    uint32_t to_read    = (n < remaining) ? n : remaining;
    uint32_t done       = 0;

    while (done < to_read) {
        uint32_t file_off  = open_files[fd].position + done;
        uint32_t block_idx = file_off / FS_BLOCK_SIZE;
        uint32_t block_off = file_off % FS_BLOCK_SIZE;
        if (block_idx >= FS_DIRECT_PTRS) break;

        uint32_t blk = ino.direct[block_idx];
        if (blk == 0) break;

        uint8_t blockbuf[FS_BLOCK_SIZE];
        block_read(blk, blockbuf);

        uint32_t chunk = FS_BLOCK_SIZE - block_off;
        if (chunk > to_read - done) chunk = to_read - done;

        for (uint32_t i = 0; i < chunk; i++) buf[done + i] = blockbuf[block_off + i];
        done += chunk;
    }

    open_files[fd].position += done;
    return (int)done;
}

int fs_write(int fd, const uint8_t *buf, uint32_t n) {
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES || !open_files[fd].used) return -1;

    inode_t ino;
    inode_load((uint32_t)open_files[fd].inode_num, &ino);

    uint32_t written = 0;
    while (written < n) {
        uint32_t file_off  = ino.size + written;
        uint32_t block_idx = file_off / FS_BLOCK_SIZE;
        uint32_t block_off = file_off % FS_BLOCK_SIZE;
        if (block_idx >= FS_DIRECT_PTRS) break;

        uint32_t blk = ino.direct[block_idx];
        if (blk == 0) {
            blk = fs_bitmap_alloc();
            if (blk == 0) break;
            ino.direct[block_idx] = blk;
        }

        uint8_t blockbuf[FS_BLOCK_SIZE];
        block_read(blk, blockbuf);

        uint32_t chunk = FS_BLOCK_SIZE - block_off;
        if (chunk > n - written) chunk = n - written;

        for (uint32_t i = 0; i < chunk; i++) blockbuf[block_off + i] = buf[written + i];
        block_write(blk, blockbuf);

        written += chunk;
    }

    ino.size += written;
    inode_save((uint32_t)open_files[fd].inode_num, &ino);

    return (int)written;
}

void fs_close(int fd) {
    if (fd < 0 || fd >= FS_MAX_OPEN_FILES) return;
    open_files[fd].used = 0;
}

int fs_unlink(const char *name) {
    dirent_t d;
    for (uint32_t i = 0; i < FS_MAX_DIRENTS; i++) {
        dirent_load(i, &d);
        if (d.inode_num != 0 && fs_strcmp(d.name, name) == 0) {
            uint32_t inode_idx = d.inode_num - 1;
            inode_t ino;
            inode_load(inode_idx, &ino);

            for (int k = 0; k < FS_DIRECT_PTRS; k++) {
                if (ino.direct[k]) fs_bitmap_set(ino.direct[k], 0);
            }

            ino.type = 0;
            ino.size = 0;
            for (int k = 0; k < FS_DIRECT_PTRS; k++) ino.direct[k] = 0;
            inode_save(inode_idx, &ino);

            d.inode_num = 0;
            d.name[0]   = '\0';
            dirent_save(i, &d);
            return 0;
        }
    }
    return -1;
}

void fs_list(fs_list_cb cb) {
    dirent_t d;
    inode_t  ino;
    for (uint32_t i = 0; i < FS_MAX_DIRENTS; i++) {
        dirent_load(i, &d);
        if (d.inode_num != 0) {
            inode_load(d.inode_num - 1, &ino);
            cb(d.name, ino.size);
        }
    }
}
