/*
* (C) 2024, Evgeny Kudryashov
* MyOS RISC-V
*/

/* OS loader
*/

#include <asm/asm.h>

    .section .text.enter , "ax"
    .global _start, start_entry, _hart_start

_start:

start_entry:


/* I supposed after U-boot it is S-mode */

    /* Disable interrapts */
    csrw sie, zero
    csrw sip, zero

//    load_global_pointer

    /* Disable FPU & VECTOR */
    li t0, (0x6000 | 0x600)
    csrc sstatus, t0

	la a2, boot_cpu_hartid
	sd a0, (a2)

    /*  Set stack */
    la t0, _stack_top   /* Stack base */
    li t1, 4096     /* Stack size */
    mul t1, a0, t1  /* a0 is number oh HART */
    add t0, t0, t1  
    li t1, 4096     /* reserved stack */
    add t0, t0, t1
    mv sp, t0


//    li sp, 0x40210000

    call boot_start
    tail boot_start

_hart_start:

    /* Disable interrapts */
    csrw sie, zero
    csrw sip, zero

        /*  Set stack */
    la t0, _stack_top   /* Stack base */
    li t1, 4096     /* Stack size */
    mul t1, a0, t1  /* a0 is number oh HART */
    add t0, t0, t1  
    li t1, 4096     /* reserved stack */
    add t0, t0, t1
    mv sp, t0

    call boot_init_hart
    tail boot_init_hart