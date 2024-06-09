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


    li a0, 65	# A
    call uart_write
    li a0, 10	# \n
    call uart_write


/* I supposed after U-boot it is S-mode */

    /* Disable interrapts */
    csrw sie, zero
    csrw sip, zero

    load_global_pointer

    /* Disable FPU & VECTOR */
    li t0, (0x6000 | 0x600)

    csrc sstatus, t0

    li sp, 0x40210000

    call boot_start


    li a0, 66	# B
    call uart_write
    li a0, 10	# \n
    call uart_write


    tail boot_start


.equ UART0,		0x10000000
.equ UART_THR,		0x00000000
.equ UART_TX_FULL,	0x80000000
.equ UART_TX_DATA,	0x000000FF

uart_write:
    li t0, UART0
    li t1, UART_TX_FULL
1:  lw t2, UART_THR(t0)
    and t3,t2,t1
    bnez t3, 1b
    andi a0, a0, UART_TX_DATA
    sw a0, UART_THR(t0)
    ret
