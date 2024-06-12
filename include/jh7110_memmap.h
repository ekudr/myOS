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

#define JH7110_DDR_BASE   (0x40200000)
#define JH7110_DDR_SIZE   (0x200000000)

/* Kernel code memory (RX) */

#define KSRAM_START     (uintptr_t)__ksram_start
#define KSRAM_SIZE      (uintptr_t)__ksram_size
#define KSRAM_END       (uintptr_t)__ksram_end

/* Kernel RAM (RW) */

#define PGPOOL_START    (uintptr_t)__pgheap_start
#define PGPOOL_SIZE     (uintptr_t)__pgheap_size

/* Page pool (RWX) */

#define PGPOOL_START    (uintptr_t)__pgheap_start
#define PGPOOL_SIZE     (uintptr_t)__pgheap_size
#define PGPOOL_END      (PGPOOL_START + PGPOOL_SIZE)


#define KSTART      (uintptr_t)__start
#define STACK_TOP       (uintptr_t)__stack_top
#define BSS_START     (uintptr_t)__bss_start
#define BSS_END       (uintptr_t)__bss_end


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

extern uint8_t      __start[];
extern uint8_t      __stack_top[];
extern uint8_t      __bss_start[];
extern uint8_t      __bss_end[];

#endif /* _JH7110_MEMMAP_H */