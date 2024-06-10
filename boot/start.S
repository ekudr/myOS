/*
* (C) 2024, Evgeny Kudryashov
* MyOS RISC-V
*/

/* OS loader
*/

#include <asm/asm.h>

    .section .text.enter
    .global _start, start_entry, uart_write

_start:

start_entry:


/* I supposed after U-boot it is S-mode */

    /* Disable interrapts */
    csrw sie, zero
    csrw sip, zero

    load_global_pointer

    /* Disable FPU & VECTOR */
    li t0, (0x6000 | 0x600)
    csrc sstatus, t0

	la a2, boot_cpu_hartid
	sw a0, (a2)

    li sp, 0x40210000

    call boot_start

    tail boot_start

