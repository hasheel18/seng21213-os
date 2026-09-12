# SENG21213-OS — Stage 0: Kernel Foundations

> **Course**: SENG 21213 – Computer Architecture & Operating Systems  
> **Year**: 2nd Year, Software Engineering  
> **Assignment**: Build your own x86 Operating System

---

## What Is This?

This is **Stage 0** of your semester-long OS assignment. Over 5 lecture milestones
(Lectures 8–12), your team will transform this minimal kernel into a functioning
operating system with process management, threading, memory management, and a
file system.

```
seng21213-os/
├── boot/
│   └── boot.asm          ← MBR Bootloader (NASM, 16-bit → 32-bit transition)
├── kernel/
│   ├── kernel_entry.asm  ← Protected-mode entry, calls kernel_main()
│   ├── kernel.c          ← Main kernel: shell loop, command dispatch
│   ├── vga.c / vga.h     ← VGA 80×25 text-mode driver
│   ├── keyboard.c / .h   ← PS/2 keyboard polling driver
├── include/
│   └── types.h           ← Primitive types (no libc!)
├── linker.ld             ← Linker script (kernel at 0x10000)
├── Makefile              ← Build system
├── Dockerfile            ← Reproducible build environment
└── README.md             ← You are here
```

---

## Milestone Schedule

| Lecture | Milestone | Files to Add |
|---------|-----------|-------------|
| L08 | ✅ Stage 0 – Boot + VGA + Shell | *Given to you* |
| L09 | Process Management | `kernel/process.c`, `kernel/scheduler.c` |
| L10 | Threads & Synchronisation | `kernel/thread.c`, `kernel/mutex.c` |
| L11 | Memory Management | `kernel/pmm.c`, `kernel/vmm.c` |
| L12 | File System | `kernel/fs.c`, `kernel/ramdisk.c` |

---

## Quick Start

### Option A: Docker (Recommended for all platforms)

```bash
# 1. Install Docker Desktop (Windows/Mac) or Docker Engine (Linux)
# 2. Build the image once:
docker build -t seng21213-os-builder .

# 3. Build the OS:
docker run --rm -v "$(pwd)":/os seng21213-os-builder

# 4. Run in QEMU (install QEMU locally):
qemu-system-i386 -drive format=raw,file=seng21213-os.img -m 32M
```

### Option B: Native Linux/WSL2

```bash
# Ubuntu/Debian
sudo apt install nasm gcc gcc-multilib binutils qemu-system-x86 make

# Build
make all

# Run
make run
```

### Option C: macOS (Homebrew)

```bash
brew install nasm x86_64-elf-binutils qemu

# You also need an i686-elf-gcc cross-compiler:
# See: https://wiki.osdev.org/GCC_Cross-Compiler
make all
make run
```

---

## Understanding the Boot Process

```
Power On
  │
  ▼
BIOS (firmware in ROM)
  │  Loads 512-byte MBR from disk sector 1 into RAM at 0x7C00
  ▼
boot/boot.asm  (Real Mode, 16-bit)
  │  Prints "Loading SENG21213-OS..."
  │  Reads 64 sectors (kernel) from disk into RAM at 0x10000
  │  Sets up GDT (Global Descriptor Table)
  │  Switches CPU to 32-bit Protected Mode
  │  Far-jumps to 0x10000
  ▼
kernel/kernel_entry.asm  (Protected Mode, 32-bit)
  │  Calls kernel_main()
  ▼
kernel/kernel.c  →  kernel_main()
  │  vga_init()     – set up text display
  │  kb_init()      – set up keyboard
  │  print_splash() – welcome screen
  │  shell_run()    – interactive shell (infinite loop)
  ▼
Your code from here...
```

---

## Building Lecture 9: Process Management

When you reach Lecture 9, you'll add process support. Here's the interface to implement:

```c
/* kernel/process.h  — you write this! */

#define MAX_PROCESSES    16
#define STACK_SIZE     4096

typedef enum { READY, RUNNING, BLOCKED, TERMINATED } proc_state_t;

typedef struct pcb {
    uint32_t      pid;
    proc_state_t  state;
    uint32_t      esp;          /* Saved stack pointer */
    uint32_t      eip;          /* Saved instruction pointer */
    uint32_t      stack[STACK_SIZE / 4];
    struct pcb   *next;         /* For linked-list ready queue */
} pcb_t;

void   process_init(void);
pcb_t *process_create(void (*entry)(void));
void   process_yield(void);        /* Trigger context switch */
void   process_exit(void);
void   scheduler_tick(void);       /* Called by timer IRQ (Lecture 10) */
```

---

## Debugging Tips

```bash
# Debug with GDB
make run-debug
# In another terminal:
gdb
(gdb) target remote :1234
(gdb) set architecture i386
(gdb) symbol-file build/kernel.elf
(gdb) break kernel_main
(gdb) continue

# Inspect the disk image
xxd seng21213-os.img | head -32    # View MBR
xxd seng21213-os.img | grep -c aa55  # Verify boot signature
```

---

## Key Learning Resources

| Topic | Reference |
|-------|-----------|
| x86 Protected Mode | Intel IA-32 Manual, Vol 3, Chapter 3 |
| VGA Text Mode | OSDev Wiki: Text UI |
| Interrupts / IDT | Stallings Ch.1; OSDev: IDT |
| Process Management | Stallings Ch.3–4 (your lecture notes) |
| Memory Management | Stallings Ch.7–8 (your lecture notes) |
| OSDev community | https://wiki.osdev.org |

---

## Assessment Rubric (per milestone)

| Criterion | Weight |
|-----------|--------|
| Code compiles and kernel boots in QEMU | 30% |
| Feature implementation (correct behaviour) | 40% |
| Code quality and comments | 20% |
| Lab demo and viva questions | 10% |

---

*Happy hacking! Remember: every commercial OS started exactly like this.*

---

## Stage 1 — Process Table & Round-Robin Scheduler (Lecture 9)

**Status:** ✅ Complete — tagged `v0.2-stage1`

### What was implemented

- **`kernel/idt.h` / `kernel/idt.c`** — Interrupt Descriptor Table setup and 8259 PIC
  remapping (IRQ0–15 moved to interrupt vectors 32–47 to avoid clashing with CPU
  exception vectors). Required infrastructure not present in the Stage 0 starter kit.
- **`kernel/isr_stub.asm`** — Assembly entry point for the IRQ0 (timer) interrupt.
- **`kernel/irq.h` / `kernel/irq.c`** — Programs the 8253/8254 PIT to fire IRQ0 at
  100 Hz; the handler calls `schedule()` on every tick.
- **`kernel/process.h` / `kernel/process.c`** — `pcb_t` process control block
  (pid, state, esp, eip, priority, name) and `create_process()`, which allocates a
  4 KB stack per process and fabricates an initial stack frame so the first
  context switch lands directly on the process's entry function.
- **`kernel/scheduler.h` / `kernel/scheduler.c`** — Round-robin scheduler.
  `scheduler_start()` performs the initial handoff from `kernel_main` into the
  first process; `schedule()` performs subsequent preemptive switches on each
  timer tick.
- **`boot/switch.asm`** — `context_switch(pcb_t *cur, pcb_t *next)`: saves the
  outgoing process's ESP into its PCB, loads ESP from the incoming process's PCB,
  and resumes execution via `POPAD; RET`.
- **`kernel/kernel.c`** — Three processes are created at boot: `shell` (priority 2,
  runs the interactive shell), `process_a` and `process_b` (priority 1, each print
  a different character at a different rate to demonstrate true concurrent
  execution under the scheduler).
- **`ps` shell command** — lists all PCBs with PID, state, priority, and name.

### How to test

```bash
make clean && make all
make run
```

- Splash screen should read "Stage 1: Process Table & Round-Robin Scheduler".
- The `A` (green) and `B` (cyan) characters cycle inside the top banner at
  different speeds, proving two processes are running concurrently, preempted by
  the 100 Hz timer interrupt.
- Type `ps` at the `ksh>` prompt — lists all three processes with correct states.
- The shell remains fully responsive while `process_a`/`process_b` keep running
  in the background (true preemptive multitasking, not cooperative).

### Implementation notes / things that tripped us up

Two non-obvious bugs came up during development, worth documenting since they're
common pitfalls in this kind of software context switch:

1. **Initial handoff from `kernel_main`.** The scheduler must not passively wait
   for the first timer interrupt to switch into the first process — the CPU is
   still running on `kernel_main`'s own boot stack at that point, and the first
   process's PCB has never been genuinely "current." `scheduler_start()` performs
   an explicit `context_switch()` from a throwaway boot context into the first
   real process before enabling interrupts.
2. **Interrupt flag on first run.** A context switch into a brand-new process
   uses a plain `RET` (not `IRET`), which does not restore EFLAGS. Since the CPU
   disables interrupts automatically when an interrupt gate is entered, a newly
   started process would otherwise run with interrupts permanently off, freezing
   the scheduler. Each process explicitly re-enables interrupts (`sti`) as its
   first instruction to guarantee preemption keeps working regardless of whether
   it's running for the first time or resuming.

---

## Stage 2 — Threads, Mutex & Semaphore (Lecture 10)

**Status:** ✅ Complete — tagged `v0.3-stage2`

### What was implemented

- **`kernel/thread.h` / `kernel/thread.c`** — `create_thread()`. This kernel has a
  single flat address space (no paging yet — see Stage 3), so "threads sharing
  the process's address space" (L10 1.1) is automatically true for every
  schedulable entity. Threads reuse the exact same PCB / scheduler /
  `context_switch` machinery from Stage 1, tagged with `pcb_t.parent_pid` so
  `ps`/`threads` can distinguish top-level processes from threads spawned by them.
- **`kernel/mutex.h` / `kernel/mutex.c`** — binary mutex. Uses `cli`/`sti` around
  its own bookkeeping as the uniprocessor mutual-exclusion primitive (L10 3.2
  covers CAS/XCHG for multi-core hardware; disabling interrupts is the
  single-core equivalent, valid since QEMU is given one CPU core here). A
  blocked thread is marked `PROC_BLOCKED`, which the Stage 1 round-robin
  scheduler already skips; `mutex_unlock()` hands the lock directly to the next
  waiter to avoid re-triggering the race it's protecting against.
- **`kernel/semaphore.h` / `kernel/semaphore.c`** — counting semaphore,
  `sem_wait()` / `sem_signal()`, same blocking mechanism as the mutex.
- **`race` / `race_mutex` shell commands** — the `myglobal` race condition demo
  from L10 3.4, ported into two kernel threads doing a non-atomic
  READ-MODIFY-WRITE on a shared counter, run first without protection (visibly
  loses updates) and then with the mutex (deterministically correct).
- **`pc` shell command** — producer-consumer with a bounded buffer (size 5),
  using three semaphores (`e` = empty slots, `n` = full slots, `s` = mutex),
  matching the naming convention in the lecture notes.

### How to test

```bash
make clean && make all
make run
```

At `ksh>`:
- `race` — repeat a few times; `myglobal` frequently prints below 40 (lost
  updates from the unprotected critical section).
- `race_mutex` — always prints exactly `myglobal = 40`.
- `pc` — always prints `Produced: 10   Consumed: 10` with no corruption.
- `threads` — lists the spawned thread PCBs after running the demos above.

### Known simplification

Because paging/virtual memory isn't implemented until Stage 3, "threads sharing
an address space" and "processes with separate address spaces" aren't currently
distinguishable in practice — everything lives in one physical address space.
`parent_pid` is used purely as bookkeeping to reflect the logical
process/thread relationship from L10, not as an enforced memory boundary.

---

## Stage 3 — Physical Memory Manager (Lecture 11)

**Status:** ✅ Complete — tagged `v0.4-stage3`

### What was implemented

- **`boot/boot.asm`** — added a `detect_memory` subroutine using BIOS
  `INT 0x15, EAX=0xE820` to enumerate the real memory map before switching to
  protected mode (this must happen in real mode, so it's the one part of
  Stage 3 that lives in the bootloader rather than the kernel). Results are
  written to a fixed buffer at physical address `0x8000`: a `uint32_t` entry
  count followed by an array of 24-byte E820 entries. The boot sector remained
  within the mandatory 512-byte MBR limit with 4 bytes to spare.
- **`kernel/pmm.h` / `kernel/pmm.c`** — bitmap physical frame allocator, one
  bit per 4 KB frame. `pmm_init()` reads the total usable memory (computed
  from the E820 table by `kernel.c`'s `detect_memory_size()`) and reserves
  the first 1 MB (frames 0–255) unconditionally — this safely covers the
  real-mode IVT, BIOS data area, the E820 buffer itself, video memory, and
  the kernel image (loaded at `0x10000`, nowhere near 1 MB). `pmm_alloc_frame()`
  does a first-fit scan; `pmm_free_frame()` clears the corresponding bit.
- **`kernel/process.c`** — process stacks are now allocated via
  `pmm_alloc_frame()` instead of a static array, so the PMM is genuinely
  load-bearing rather than a side feature: every process and thread created
  from Stage 1 onward now draws its 4 KB stack from the physical memory
  manager.
- **`meminfo` shell command** — prints total/used/free frames and KB.
- **`pmmtest` shell command** — allocates 10 frames, frees two of them,
  allocates two more, and verifies the freed frames were reused (first-fit
  behaviour), matching the milestone's required self-test.

### How to test

```bash
make clean && make all
make run
```

At `ksh>`:
- `meminfo` — shows real detected memory (not a hardcoded value — the total
  frame count reflects whatever QEMU/BIOS actually reports via E820).
- `pmmtest` — always prints `PASS: freed frames were correctly reused (first-fit)`.
- `ps` — still lists all processes correctly, confirming PMM-backed stacks
  didn't break process creation.
- `race`, `race_mutex`, `pc` (Stage 2 demos) — still behave identically,
  confirming thread creation through the same PMM-backed path works correctly.

### Implementation notes

Exact frame addresses will differ from the illustrative example in the
lecture notes (which assumes a specific pre-existing reservation), since our
reserved region size (first 1 MB / 256 frames) is a deliberate simplification
rather than a computed `KERNEL_END` symbol from the linker script — chosen to
avoid the risk of modifying `linker.ld` and because the kernel image is
comfortably smaller than 1 MB regardless of exact size. `pmmtest` verifies the
*behaviour* (reuse of freed frames) rather than checking against fixed
addresses, since those addresses are an artifact of a specific memory layout.

---

## Stage 4 — RAM Disk File System (Lecture 12)

**Status:** ✅ Complete — tagged `v0.5-stage4`

### What was implemented

- **`kernel/ramdisk.h` / `kernel/ramdisk.c`** — a 1 MB contiguous byte array
  acting as block storage (512-byte blocks, 2048 total), with `rd_read()` /
  `rd_write()`. This lives in `.bss`.
- **`linker.ld` / `kernel/kernel_entry.asm`** — `.bss` is marked `NOLOAD` so
  the 1 MB ramdisk costs zero bytes in the actual kernel binary on disk (the
  bootloader only reads 64 sectors / 32 KB). `kernel_entry.asm` zeroes `.bss`
  manually at boot before any C code runs, since it's no longer guaranteed
  zero by the loader. Verified: final `kernel.bin` is ~16 KB, comfortably
  under the 32 KB load budget.
- **`kernel/pmm.c`** — reserved region increased from 1 MB to 2 MB to
  safely cover the kernel image plus the new ramdisk.
- **`kernel/fs.h` / `kernel/fs.c`** — i-node-style file system on top of the
  ramdisk: superblock (block 0), flat directory of 16 entries (block 1),
  inode table of 48 inodes across 4 blocks, a free-block bitmap (block 6),
  and data blocks from block 7 onward. All metadata is read/written directly
  from the ramdisk on every operation (not cached in kernel globals),
  matching how a real indexed allocation file system works (L12 1.2).
  Implements `fs_init`, `fs_create`, `fs_open`, `fs_read`, `fs_write`
  (append semantics), `fs_close`, `fs_unlink`, `fs_list`.
- **Shell commands** — `ls`, `touch <name>`, `cat <name>`,
  `write <name> <text>` (append), `rm <name>`.

### How to test

```bash
make clean && make all
make run
```

At `ksh>`:

touch hello.txt
ls
write hello.txt Hello, SENG OS!
cat hello.txt
rm hello.txt
ls

Matches the milestone's required test sequence exactly.

### Known simplifications

- Max file size is 4 KB (8 direct block pointers × 512 bytes) — no indirect
  block support, unlike the full i-node indirection described in L12 1.2.
  Documented as a possible extension rather than implemented, given time
  constraints.
- Flat directory only (16 files max) — no subdirectories.
- `fs_write` always appends; there's no seek/overwrite-in-place.
