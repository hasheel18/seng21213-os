#ifndef FS_H
#define FS_H
#include "../include/types.h"

#define FS_MAGIC        0x53454E47u
#define FS_BLOCK_SIZE   512u
#define FS_TOTAL_BLOCKS 2048u
#define FS_MAX_NAME     28
#define FS_DIRECT_PTRS  8
#define FS_MAX_INODES   48
#define FS_MAX_DIRENTS  16
#define FS_MAX_FILE_SIZE (FS_DIRECT_PTRS * FS_BLOCK_SIZE)
#define FS_MAX_OPEN_FILES 8

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint32_t block_count;
    uint32_t inode_count;
    uint32_t free_bitmap_offset;
} superblock_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t size;
    uint32_t direct[FS_DIRECT_PTRS];
} inode_t;

typedef struct __attribute__((packed)) {
    uint32_t inode_num;
    char     name[FS_MAX_NAME];
} dirent_t;

typedef void (*fs_list_cb)(const char *name, uint32_t size);

void fs_init(void);
int  fs_create(const char *name);
int  fs_open(const char *name);
int  fs_read(int fd, uint8_t *buf, uint32_t n);
int  fs_write(int fd, const uint8_t *buf, uint32_t n);
void fs_close(int fd);
int  fs_unlink(const char *name);
void fs_list(fs_list_cb cb);

#endif
