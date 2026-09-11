/* =============================================================================
 * SENG21213-OS :: Main Kernel  (Stage 3 - Physical Memory Manager)
 * File   : kernel/kernel.c
 * ============================================================================*/

#include "vga.h"
#include "keyboard.h"
#include "../include/types.h"
#include "idt.h"
#include "process.h"
#include "scheduler.h"
#include "irq.h"
#include "thread.h"
#include "mutex.h"
#include "semaphore.h"
#include "pmm.h"

static void cmd_help(void);
static void cmd_clear(void);
static void cmd_about(void);
static void cmd_echo(const char *args);
static void cmd_ps(void);
static void cmd_threads(void);
static void cmd_race(int use_mutex);
static void cmd_pc(void);
static void cmd_meminfo(void);
static void cmd_pmmtest(void);

static int k_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (uint8_t)*a - (uint8_t)*b;
}

static int k_strncmp(const char *a, const char *b, size_t n) {
    while (n-- && *a && (*a == *b)) { a++; b++; }
    return n == (size_t)-1 ? 0 : (uint8_t)*a - (uint8_t)*b;
}

static size_t k_strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static const char *k_ltrim(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* ---------------------------------------------------------------------------
 * Stage 3: BIOS E820 memory map, read from the fixed buffer the bootloader
 * filled in before switching to protected mode (boot/boot.asm). (L11 4.1)
 * --------------------------------------------------------------------------*/
typedef struct __attribute__((packed)) {
    uint32_t base_low, base_high;
    uint32_t length_low, length_high;
    uint32_t type;
    uint32_t acpi_ext;
} e820_entry_t;

#define E820_TYPE_USABLE 1

static uint32_t detect_memory_size(void) {
    uint32_t count = *(volatile uint32_t *)0x8000;
    e820_entry_t *entries = (e820_entry_t *)0x8004;
    uint32_t total = 0;

    for (uint32_t i = 0; i < count; i++) {
        if (entries[i].type == E820_TYPE_USABLE && entries[i].base_high == 0) {
            total += entries[i].length_low;
        }
    }

    if (total == 0) total = 32u * 1024u * 1024u;  /* fallback if E820 unsupported */
    return total;
}

/* ---------------------------------------------------------------------------
 * Stage 1 demo processes
 * --------------------------------------------------------------------------*/
static void process_a(void) {
    __asm__ __volatile__("sti");
    int col = 0;
    while (1) {
        VGA_ADDR[6 * VGA_COLS + col] =
            (uint16_t)((VGA_ATTR(VGA_LIGHT_GREEN, VGA_BLACK) << 8) | 'A');
        col = (col + 1) % VGA_COLS;
        for (volatile int d = 0; d < 2000000; d++) { }
    }
}

static void process_b(void) {
    __asm__ __volatile__("sti");
    int col = 0;
    while (1) {
        VGA_ADDR[6 * VGA_COLS + 40 + col % 40] =
            (uint16_t)((VGA_ATTR(VGA_LIGHT_CYAN, VGA_BLACK) << 8) | 'B');
        col = (col + 1) % VGA_COLS;
        for (volatile int d = 0; d < 800000; d++) { }
    }
}

/* ---------------------------------------------------------------------------
 * Stage 2: race condition + producer-consumer demos
 * --------------------------------------------------------------------------*/
static volatile int myglobal = 0;
static volatile int race_threads_done = 0;
static int race_use_mutex = 0;
static mutex_t global_mutex;

static void race_thread(void) {
    __asm__ __volatile__("sti");
    for (int i = 0; i < 20; i++) {
        if (race_use_mutex) mutex_lock(&global_mutex);
        int j = myglobal;
        j = j + 1;
        for (volatile int d = 0; d < 300000; d++) { }
        myglobal = j;
        if (race_use_mutex) mutex_unlock(&global_mutex);
    }
    race_threads_done++;
    pcb_t *cur = scheduler_current();
    if (cur) cur->state = PROC_ZOMBIE;
    while (1) { schedule(); }
}

static void cmd_race(int use_mutex) {
    myglobal = 0;
    race_threads_done = 0;
    race_use_mutex = use_mutex;
    mutex_init(&global_mutex);

    create_thread("race1", race_thread, 1, 1);
    create_thread("race2", race_thread, 1, 1);

    vga_puts_color(use_mutex
        ? "\n  Running race demo WITH mutex protection...\n"
        : "\n  Running race demo WITHOUT protection (expect corruption)...\n",
        VGA_YELLOW, VGA_BLACK);

    while (race_threads_done < 2) { }

    vga_printf("  myglobal = %d   (expected 40)\n", myglobal);
    vga_puts("\n");
}

#define PC_BUF_SIZE 5
static int pc_buffer[PC_BUF_SIZE];
static int pc_in = 0, pc_out = 0;
static sem_t pc_e, pc_n, pc_s;
static volatile int pc_produced = 0, pc_consumed = 0, pc_done = 0;

static void producer_thread(void) {
    __asm__ __volatile__("sti");
    for (int item = 1; item <= 10; item++) {
        sem_wait(&pc_e);
        sem_wait(&pc_s);
        pc_buffer[pc_in] = item;
        pc_in = (pc_in + 1) % PC_BUF_SIZE;
        pc_produced++;
        sem_signal(&pc_s);
        sem_signal(&pc_n);
        for (volatile int d = 0; d < 500000; d++) { }
    }
    pcb_t *cur = scheduler_current();
    if (cur) cur->state = PROC_ZOMBIE;
    while (1) { schedule(); }
}

static void consumer_thread(void) {
    __asm__ __volatile__("sti");
    for (int i = 0; i < 10; i++) {
        sem_wait(&pc_n);
        sem_wait(&pc_s);
        int item = pc_buffer[pc_out];
        pc_out = (pc_out + 1) % PC_BUF_SIZE;
        pc_consumed++;
        (void)item;
        sem_signal(&pc_s);
        sem_signal(&pc_e);
        for (volatile int d = 0; d < 700000; d++) { }
    }
    pc_done = 1;
    pcb_t *cur = scheduler_current();
    if (cur) cur->state = PROC_ZOMBIE;
    while (1) { schedule(); }
}

static void cmd_pc(void) {
    pc_in = 0; pc_out = 0;
    pc_produced = 0; pc_consumed = 0; pc_done = 0;

    sem_init(&pc_e, PC_BUF_SIZE);
    sem_init(&pc_n, 0);
    sem_init(&pc_s, 1);

    create_thread("producer", producer_thread, 1, 1);
    create_thread("consumer", consumer_thread, 1, 1);

    vga_puts_color("\n  Running producer-consumer demo (buffer size 5)...\n",
                   VGA_YELLOW, VGA_BLACK);

    while (!pc_done) { }

    vga_printf("  Produced: %d   Consumed: %d   (expected 10 / 10, no corruption)\n",
               pc_produced, pc_consumed);
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Splash Screen
 * --------------------------------------------------------------------------*/
static void print_splash(void) {
    vga_clear(VGA_BLACK);
    vga_draw_box(0, 0, 7, 80, VGA_LIGHT_MAGENTA);

    vga_set_cursor(1, 2);
    vga_puts_color("  SENG21213-OS  |  Computer Architecture & Operating Systems",
                   VGA_YELLOW, VGA_BLACK);

    vga_set_cursor(2, 2);
    vga_puts_color("  Stage 3: Physical Memory Manager", VGA_LIGHT_CYAN, VGA_BLACK);

    vga_set_cursor(3, 2);
    vga_puts_color("  Faculty of Engineering - Department of Software Engineering",
                   VGA_LIGHT_GREY, VGA_BLACK);

    vga_set_cursor(4, 2);
    vga_puts_color("  Built by students, for students.  Type 'help' to begin.",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    vga_set_cursor(5, 2);
    vga_puts_color("  CPU: i686 (32-bit Protected Mode)  |  Display: VGA 80x25",
                   VGA_DARK_GREY, VGA_BLACK);

    vga_set_cursor(8, 0);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_puts("  Welcome! This kernel was compiled from source and booted entirely\n");
    vga_puts("  from bare metal. There is no Linux or Windows underneath - only\n");
    vga_puts("  the code you and your team write.\n");
    vga_puts("\n");
    vga_puts("  Assignment milestones to implement:\n");
    vga_puts_color("    [L09] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Process Management  - DONE: PCB, ready queue, round-robin scheduler\n");
    vga_puts_color("    [L10] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Threads & Sync      - DONE: threads, mutex, semaphore\n");
    vga_puts_color("    [L11] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("Memory Management   - DONE: E820 detection, bitmap frame allocator\n");
    vga_puts_color("    [L12] ", VGA_YELLOW, VGA_BLACK);
    vga_puts("File System         - RAM disk, FAT-like directory structure\n");
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell command implementations
 * --------------------------------------------------------------------------*/
static void cmd_help(void) {
    vga_puts_color("\n  SENG21213-OS Shell Commands\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_puts("  help       - Show this help message\n");
    vga_puts("  clear      - Clear the screen\n");
    vga_puts("  about      - About this OS and course\n");
    vga_puts("  echo       - Echo text to screen\n");
    vga_puts("  ps         - [L09] List processes\n");
    vga_puts("  threads    - [L10] List kernel threads\n");
    vga_puts("  race       - [L10] Race condition demo (no mutex)\n");
    vga_puts("  race_mutex - [L10] Same demo, WITH mutex protection\n");
    vga_puts("  pc         - [L10] Producer-consumer demo (semaphores)\n");
    vga_puts("  meminfo    - [L11] Show physical memory usage\n");
    vga_puts("  pmmtest    - [L11] PMM self-test (alloc/free/reuse)\n");
    vga_puts_color("\n  Milestones (to implement):\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  kill    - [L09] Terminate a process\n");
    vga_puts("  ls      - [L12] List files\n");
    vga_puts("  cat     - [L12] Print file contents\n\n");
}

static void cmd_clear(void) {
    vga_clear(VGA_BLACK);
}

static void cmd_about(void) {
    vga_puts_color("\n  About SENG21213-OS\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_puts("  Architecture : x86 (i686), 32-bit Protected Mode\n");
    vga_puts("  Bootloader   : Custom MBR (NASM)\n");
    vga_puts("  Kernel       : Freestanding C (GCC, no libc)\n");
    vga_puts("  VM Target    : QEMU (qemu-system-i386)\n");
    vga_puts("  Course       : SENG 21213 - Sem 2\n");
    vga_puts("  Reference    : Stallings, OS: Internals & Design Principles\n\n");
}

static void cmd_echo(const char *args) {
    vga_puts("  ");
    vga_puts(args);
    vga_puts("\n");
}

static void cmd_ps(void) {
    vga_puts_color("\n  PID  STATE     PRI  PARENT  NAME\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    for (int i = 0; i < process_count; i++) {
        pcb_t *p = &process_table[i];
        const char *state_str =
            p->state == PROC_READY   ? "READY"   :
            p->state == PROC_RUNNING ? "RUNNING" :
            p->state == PROC_BLOCKED ? "BLOCKED" : "ZOMBIE";
        vga_printf("  %d    %s   %d    %d       %s\n",
                   p->pid, state_str, p->priority, p->parent_pid, p->name);
    }
    vga_puts("\n");
}

static void cmd_threads(void) {
    vga_puts_color("\n  TID  STATE     PRI  PARENT  NAME\n", VGA_YELLOW, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    int any = 0;
    for (int i = 0; i < process_count; i++) {
        pcb_t *p = &process_table[i];
        if (p->parent_pid == 0) continue;
        any = 1;
        const char *state_str =
            p->state == PROC_READY   ? "READY"   :
            p->state == PROC_RUNNING ? "RUNNING" :
            p->state == PROC_BLOCKED ? "BLOCKED" : "ZOMBIE";
        vga_printf("  %d    %s   %d    %d       %s\n",
                   p->pid, state_str, p->priority, p->parent_pid, p->name);
    }
    if (!any) vga_puts("  (no threads yet - try 'race', 'race_mutex', or 'pc')\n");
    vga_puts("\n");
}

static void cmd_meminfo(void) {
    uint32_t total = pmm_total_frames();
    uint32_t used  = pmm_used_frames();
    uint32_t free_ = pmm_free_frames();

    vga_puts_color("\n  Physical Memory Manager (L11)\n", VGA_LIGHT_CYAN, VGA_BLACK);
    vga_puts("  -----------------------------------------------\n");
    vga_printf("  Total : %d frames  (%d KB)\n", total, total * 4);
    vga_printf("  Used  : %d frames  (%d KB)\n", used, used * 4);
    vga_printf("  Free  : %d frames  (%d KB)\n", free_, free_ * 4);
    vga_puts("\n");
}

static void cmd_pmmtest(void) {
    vga_puts_color("\n  PMM self-test: allocate 10, free #3 and #7, allocate 2 more\n",
                   VGA_YELLOW, VGA_BLACK);

    uint32_t addrs[10];
    for (int i = 0; i < 10; i++) addrs[i] = pmm_alloc_frame();

    vga_printf("  First of 10 allocated: 0x%x\n", addrs[0]);

    pmm_free_frame(addrs[3]);
    pmm_free_frame(addrs[7]);

    uint32_t r1 = pmm_alloc_frame();
    uint32_t r2 = pmm_alloc_frame();

    vga_printf("  Freed:        0x%x and 0x%x\n", addrs[3], addrs[7]);
    vga_printf("  Reallocated:  0x%x and 0x%x\n", r1, r2);

    if (r1 == addrs[3] && r2 == addrs[7]) {
        vga_puts_color("  PASS: freed frames were correctly reused (first-fit)\n",
                       VGA_LIGHT_GREEN, VGA_BLACK);
    } else {
        vga_puts_color("  FAIL: reuse did not match expected frames\n",
                       VGA_LIGHT_RED, VGA_BLACK);
    }

    /* clean up so repeated test runs don't permanently eat memory */
    pmm_free_frame(r1);
    pmm_free_frame(r2);
    for (int i = 0; i < 10; i++) {
        if (i == 3 || i == 7) continue;
        pmm_free_frame(addrs[i]);
    }
    vga_puts("\n");
}

/* ---------------------------------------------------------------------------
 * Shell process
 * --------------------------------------------------------------------------*/
static char  shell_buf[256];
static char  prompt[] = "\n  ksh> ";

static void shell_run(void) {
    vga_puts_color("\n  Kernel Shell ready. Type 'help' for commands.\n",
                   VGA_LIGHT_GREEN, VGA_BLACK);

    while (true) {
        vga_puts_color(prompt, VGA_LIGHT_GREEN, VGA_BLACK);
        kb_readline(shell_buf, sizeof(shell_buf));

        const char *cmd = k_ltrim(shell_buf);
        if (k_strlen(cmd) == 0) continue;

        if (k_strcmp(cmd, "help")       == 0) { cmd_help();       continue; }
        if (k_strcmp(cmd, "clear")      == 0) { cmd_clear();      continue; }
        if (k_strcmp(cmd, "about")      == 0) { cmd_about();      continue; }
        if (k_strcmp(cmd, "ps")         == 0) { cmd_ps();         continue; }
        if (k_strcmp(cmd, "threads")    == 0) { cmd_threads();    continue; }
        if (k_strcmp(cmd, "race")       == 0) { cmd_race(0);      continue; }
        if (k_strcmp(cmd, "race_mutex") == 0) { cmd_race(1);      continue; }
        if (k_strcmp(cmd, "pc")         == 0) { cmd_pc();         continue; }
        if (k_strcmp(cmd, "meminfo")    == 0) { cmd_meminfo();    continue; }
        if (k_strcmp(cmd, "pmmtest")    == 0) { cmd_pmmtest();    continue; }

        if (k_strncmp(cmd, "echo ", 5) == 0) {
            cmd_echo(k_ltrim(cmd + 5));
            continue;
        }

        if (k_strcmp(cmd, "kill") == 0 ||
            k_strcmp(cmd, "ls")   == 0 ||
            k_strcmp(cmd, "cat")  == 0) {
            vga_puts_color("  [TODO] This command is not yet implemented.\n",
                           VGA_YELLOW, VGA_BLACK);
            vga_puts("  Implement it as part of your lecture assignment.\n");
            continue;
        }

        vga_puts_color("  Unknown command: ", VGA_LIGHT_RED, VGA_BLACK);
        vga_puts(cmd);
        vga_puts("\n  Type 'help' for a list of commands.\n");
    }
}

static void shell_entry(void) {
    pit_init(100);
    __asm__ __volatile__("sti");
    print_splash();
    shell_run();
}

void kernel_main(void) {
    vga_init();
    kb_init();
    idt_init();

    pmm_init(detect_memory_size());   /* must run before any create_process() call */

    create_process("shell",     shell_entry, 2);
    create_process("process_a", process_a,   1);
    create_process("process_b", process_b,   1);
    scheduler_init();
    scheduler_start();

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
