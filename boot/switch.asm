[BITS 32]
[GLOBAL context_switch]

context_switch:
    pushad

    mov eax, [esp+36]
    mov [eax+8], esp

    mov eax, [esp+40]
    mov esp, [eax+8]

    popad
    ret
