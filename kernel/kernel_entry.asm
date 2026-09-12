[BITS 32]
[EXTERN kernel_main]
[EXTERN _bss_start]
[EXTERN _bss_end]
[GLOBAL _start]

_start:
    mov  edi, _bss_start
    mov  ecx, _bss_end
    sub  ecx, edi
    xor  eax, eax
    cld
    rep  stosb

    call kernel_main

    cli
.halt:
    hlt
    jmp .halt
