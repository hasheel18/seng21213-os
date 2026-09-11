; =============================================================================
; SENG21213-OS :: Bootloader
; File   : boot/boot.asm
; Purpose: MBR (Master Boot Record) bootloader. Detects the BIOS memory map
;          (L11/Stage 3), switches CPU from 16-bit Real Mode to 32-bit
;          Protected Mode, then loads and jumps to the kernel.
; =============================================================================

[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor  ax, ax
    mov  ds, ax
    mov  es, ax
    mov  ss, ax
    mov  sp, 0x7C00
    sti

    mov  [boot_drive], dl

    mov  si, msg_banner
    call print_rm
    mov  si, msg_load
    call print_rm

load_kernel:
    mov  bx, 0x1000
    mov  es, bx
    xor  bx, bx

    mov  ah, 0x02
    mov  al, 64
    mov  ch, 0
    mov  cl, 2
    mov  dh, 0
    mov  dl, [boot_drive]
    int  0x13
    jc   disk_error

    mov  si, msg_ok
    call print_rm

    call detect_memory

enter_pm:
    cli
    lgdt [gdt_descriptor]

    mov  eax, cr0
    or   eax, 0x1
    mov  cr0, eax

    jmp  CODE_SEG:init_pm32

[BITS 32]
init_pm32:
    mov  ax, DATA_SEG
    mov  ds, ax
    mov  ss, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax

    mov  ebp, 0x90000
    mov  esp, ebp

    call 0x10000
    hlt

[BITS 16]
disk_error:
    mov  si, msg_err
    call print_rm
    mov  si, msg_halt
    call print_rm
    jmp  $

print_rm:
    lodsb
    or   al, al
    jz   .done
    mov  ah, 0x0E
    xor  bh, bh
    int  0x10
    jmp  print_rm
.done:
    ret

; ---------------------------------------------------------------------------
; Subroutine: detect_memory - BIOS INT 0x15, EAX=0xE820 memory map (L11/Stage3)
; Stores: dword [0x8000] = entry_count, then array of 24-byte entries at 0x8004
; ---------------------------------------------------------------------------
detect_memory:
    mov  dword [0x8000], 0
    xor  ax, ax
    mov  es, ax
    mov  di, 0x8004
    xor  ebx, ebx
    xor  bp, bp
.e820_loop:
    cmp  bp, 32
    jae  .e820_done
    mov  eax, 0xE820
    mov  edx, 0x534D4150
    mov  ecx, 24
    int  0x15
    jc   .e820_done
    cmp  eax, 0x534D4150
    jne  .e820_done
    inc  bp
    add  di, 24
    test ebx, ebx
    jnz  .e820_loop
.e820_done:
    mov  [0x8000], bp
    ret

boot_drive  db 0

msg_banner  db 13, 10, '  ================================', 13, 10
            db '  SENG21213-OS  |  Stage 0        ', 13, 10
            db '  Computer Architecture & OS       ', 13, 10
            db '  ================================', 13, 10, 0
msg_load    db '  [BOOT] Loading kernel...', 13, 10, 0
msg_ok      db '  [BOOT] Kernel loaded OK ', 13, 10, 0
msg_err     db '  [BOOT] DISK ERROR!       ', 13, 10, 0
msg_halt    db '  System halted.           ', 13, 10, 0

gdt_start:
gdt_null:
    dd 0x00000000
    dd 0x00000000

gdt_code:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10011010b
    db 11001111b
    db 0x00

gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

times 510 - ($ - $$) db 0
dw 0xAA55
