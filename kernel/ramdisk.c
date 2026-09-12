/* =============================================================================
 * SENG21213-OS :: RAM Disk (L12 Section 3, Stage 4)
 * File: kernel/ramdisk.c
 *
 * A 1 MB contiguous byte array acting as block storage, 512-byte blocks
 * (2048 blocks total). This lives in .bss, which the linker marks NOLOAD
 * (see linker.ld) so it costs zero bytes in the actual kernel.bin on disk -
 * kernel_entry.asm zeroes it manually at boot before any C code runs.
 * ============================================================================*/
#include "ramdisk.h"

static uint8_t ramdisk_storage[RD_BLOCK_SIZE * RD_TOTAL_BLOCKS];

void rd_read(uint32_t block, uint8_t *buf) {
    if (block >= RD_TOTAL_BLOCKS) return;
    uint8_t *src = ramdisk_storage + (block * RD_BLOCK_SIZE);
    for (uint32_t i = 0; i < RD_BLOCK_SIZE; i++) buf[i] = src[i];
}

void rd_write(uint32_t block, const uint8_t *buf) {
    if (block >= RD_TOTAL_BLOCKS) return;
    uint8_t *dst = ramdisk_storage + (block * RD_BLOCK_SIZE);
    for (uint32_t i = 0; i < RD_BLOCK_SIZE; i++) dst[i] = buf[i];
}
