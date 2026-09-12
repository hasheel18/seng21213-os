#ifndef RAMDISK_H
#define RAMDISK_H
#include "../include/types.h"

#define RD_BLOCK_SIZE   512u
#define RD_TOTAL_BLOCKS 2048u

void rd_read(uint32_t block, uint8_t *buf);
void rd_write(uint32_t block, const uint8_t *buf);

#endif
