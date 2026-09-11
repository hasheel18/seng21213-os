/* =============================================================================
 * SENG21213-OS :: Physical Memory Manager - bitmap allocator (L11 Section 4)
 * File: kernel/pmm.c
 *
 * One bit per 4 KB physical frame. bit=1 means used, bit=0 means free.
 * The first 1 MB (frames 0-255) is unconditionally reserved: real-mode IVT,
 * BIOS data area, the E820 buffer at 0x8000, video memory, and the kernel
 * image itself, which is loaded at 0x10000 and comfortably fits under 1 MB.
 * ============================================================================*/
#include "pmm.h"

#define FRAME_SIZE        4096u
#define MAX_FRAMES        8192u
#define BITMAP_WORDS      (MAX_FRAMES / 32)
#define RESERVED_FRAMES   256u

static uint32_t bitmap[BITMAP_WORDS];
static uint32_t total_frames = 0;
static uint32_t used_frames  = 0;

static void bitmap_set(uint32_t frame)   { bitmap[frame / 32] |=  (1u << (frame % 32)); }
static void bitmap_clear(uint32_t frame) { bitmap[frame / 32] &= ~(1u << (frame % 32)); }
static int  bitmap_test(uint32_t frame)  { return (bitmap[frame / 32] >> (frame % 32)) & 1u; }

void pmm_init(uint32_t memsize_bytes) {
    total_frames = memsize_bytes / FRAME_SIZE;
    if (total_frames > MAX_FRAMES) total_frames = MAX_FRAMES;
    if (total_frames < RESERVED_FRAMES) total_frames = RESERVED_FRAMES;

    for (uint32_t i = 0; i < BITMAP_WORDS; i++) bitmap[i] = 0;
    used_frames = 0;

    for (uint32_t i = 0; i < RESERVED_FRAMES && i < total_frames; i++) {
        bitmap_set(i);
        used_frames++;
    }
}

uint32_t pmm_alloc_frame(void) {
    for (uint32_t i = 0; i < total_frames; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_frames++;
            return i * FRAME_SIZE;
        }
    }
    return 0;
}

void pmm_free_frame(uint32_t addr) {
    uint32_t frame = addr / FRAME_SIZE;
    if (frame == 0 || frame >= total_frames) return;
    if (bitmap_test(frame)) {
        bitmap_clear(frame);
        used_frames--;
    }
}

uint32_t pmm_total_frames(void) { return total_frames; }
uint32_t pmm_used_frames(void)  { return used_frames; }
uint32_t pmm_free_frames(void)  { return total_frames - used_frames; }
