#include <syscall.h>

    .global exit
exit:
    li a7, SYS_exit
    ecall
    ret

    .global putc
putc:
    li a7, SYS_putc
    ecall
    ret