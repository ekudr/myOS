/****************************************************************************
 * boards/risc-v/jh7110/star64/include/board_memorymap.h
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef _JH7110_MEMMAP_H
#define _JH7110_MEMMAP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <stdint.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* DDR start address */

#define JH7110_DDR_BASE   (0x40000000)
#define JH7110_DDR_SIZE   (0x200000000)

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 32        // Global IRQ + 5
#define UART0_REG_SHIFT 2


// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

/* PLIC Base address */
#define JH7110_PLIC_BASE 0x0c000000L

/* Interrupt Priority */
#define JH7110_PLIC_PRIORITY  (JH7110_PLIC_BASE + 0x000000)

/* Hart 1 S-Mode Interrupt Enable */
#define JH7110_PLIC_ENABLE1   (JH7110_PLIC_BASE + 0x002100)
#define JH7110_PLIC_ENABLE2   (JH7110_PLIC_BASE + 0x002104)

/* Hart 1 S-Mode Priority Threshold */
#define JH7110_PLIC_THRESHOLD (JH7110_PLIC_BASE + 0x202000)

/* Hart 1 S-Mode Claim / Complete */
#define JH7110_PLIC_CLAIM     (JH7110_PLIC_BASE + 0x202004)

#define JH7110_PLIC_SENABLE(hart) (JH7110_PLIC_BASE + 0x2100 + (hart)*0x100)



#define JH7110_PLIC_SPRIORITY(hart) (JH7110_PLIC_THRESHOLD + (hart)*0x2000)

#define JH7110_PLIC_SCLAIM(hart) (JH7110_PLIC_CLAIM + (hart)*0x2000)


#define JH7110_SDIO1_BASE 0x16020000UL


/* Kernel code memory (RX) */


#define KSTART      (uintptr_t)_start
#define STACK_TOP       (uintptr_t)_stack_top
#define BSS_START     (uintptr_t)_bss_start
#define BSS_END       (uintptr_t)_bss_end


/****************************************************************************
 * Public Data
 ****************************************************************************/


/* Kernel RAM (RW) */

extern uint8_t          __ksram_start[];
extern uint8_t          __ksram_size[];
extern uint8_t          __ksram_end[];

/* Page pool (RWX) */

extern uint8_t          __pgheap_start[];
extern uint8_t          __pgheap_size[];

extern uint8_t      _start[];
extern uint8_t      _stack_top[];
extern uint8_t      _bss_start[];
extern uint8_t      _bss_end[];

#endif /* _JH7110_MEMMAP_H */