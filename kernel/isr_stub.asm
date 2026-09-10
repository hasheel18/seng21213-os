[BITS 32]
[GLOBAL isr32]
[EXTERN irq0_handler]

isr32:
    pushad
    call irq0_handler
    popad
    iretd
