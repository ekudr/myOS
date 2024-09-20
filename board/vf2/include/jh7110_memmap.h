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

/* Map the whole I/O memory with vaddr = paddr mappings */

#define MMU_IO_BASE     (0x00000000)
#define MMU_IO_SIZE     (0x40000000)

/* DDR start address */

#define DDR_BASE   0x40000000UL
#define DDR_SIZE   0xbe000000UL // exclude framebuffer
//#define DDR_SIZE   0x200000000UL
#define DDR1_BASE   0x100000000UL
#define DDR1_SIZE   0x100000000UL

#define FB	0xfe000000UL
#define FB_SIZE	0x2000000UL

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
#define UART0_IRQ 32        // Global IRQ + 5
#define UART0_REG_SHIFT 2
#define UART0_DIV 0x0D


// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.

/* PLIC Base address */
#define PLIC_BASE 0x0c000000L

/* Interrupt Priority */
#define PLIC_PRIORITY  (PLIC_BASE + 0x000000)

/* Hart 1 S-Mode Interrupt Enable */
#define PLIC_ENABLE1   (PLIC_BASE + 0x002100)
#define PLIC_ENABLE2   (PLIC_BASE + 0x002104)

/* Hart 1 S-Mode Priority Threshold */
#define PLIC_THRESHOLD (PLIC_BASE + 0x202000)

/* Hart 1 S-Mode Claim / Complete */
#define PLIC_CLAIM     (PLIC_BASE + 0x202004)

#define PLIC_SENABLE(hart) (PLIC_BASE + 0x2100 + (hart-1)*0x100)

#define PLIC_SPRIORITY(hart) (PLIC_THRESHOLD + (hart-1)*0x2000)

#define PLIC_SCLAIM(hart) (PLIC_CLAIM + (hart-1)*0x2000)


#define SDIO1_BASE 0x16020000UL

#define AON_PINCTRL_BASE 0x17020000UL


#endif /* _JH7110_MEMMAP_H */