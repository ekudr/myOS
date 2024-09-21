#include <syscall.h>

.global exit

exit:
    li a7, SYS_exit
    ecall
    ret
