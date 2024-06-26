/****************************************************************************
 * arch/risc-v/src/jh7110/jh7110_mm_init.c
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

#include <common.h>
#include <queue.h>
#include <jh7110_memmap.h>
#include <spinlock.h>
#include <mmu.h>
#include <sched.h>


/* Map the whole I/O memory with vaddr = paddr mappings */

#define MMU_IO_BASE     (0x00000000)
#define MMU_IO_SIZE     (0x40000000)


/* Physical and virtual addresses to page tables (vaddr = paddr mapping) */




#define PGT_L1_SIZE     (512)  /* Enough to map 512 GiB */
#define PGT_L2_SIZE     (512)  /* Enough to map 1 GiB */
#define PGT_L3_SIZE     (1024) /* Enough to map 4 MiB (2MiB x 2) */

#define KMM_PAGE_SIZE   RV_MMU_L3_PAGE_SIZE
 
#define KMM_PBASE_IDX   3   

#define KMM_SPBASE_IDX  2




/* Kernel mappings simply here, mapping is vaddr=paddr */



uintptr_t   mem_start;
uintptr_t   mem_end;

extern char trampoline[], uservec[], userret[];

void kernel_mapping(void) {

  int status;
 
  /* Allocate page for L1 */
  g_kernel_pgt_base = (uintptr_t)pg_alloc();
  if (!g_kernel_pgt_base) {
    printf("[MMU] Can NOT allocate page\n");
    while (1) {}
  }
  memset(g_kernel_pgt_base, 0, RV_MMU_PAGE_SIZE);

  
    /* Map I/O region, use enough large page tables for the IO region. */

  printf("[MMU] map I/O regions\n");

  mmu_ln_map_region(1, g_kernel_pgt_base, MMU_IO_BASE, MMU_IO_BASE,
                    MMU_IO_SIZE, MMU_IO_FLAGS);

  /* Map the kernel text and data for L2/L3 */


  printf("[MMU] map kernel\n");
  status = mmu_map_pages(g_kernel_pgt_base,KSTART, mem_start-KSTART-1, KSTART, MMU_KDATA_FLAGS);
    if (status)
      panic("[MMU] map_init: can not map");
  /* Map the page pool */

  printf("[MMU] map the page pool\n");

  status = mmu_map_pages(g_kernel_pgt_base, mem_start, mem_end-mem_start, mem_start, MMU_KDATA_FLAGS);
    if (status)
      panic("[MMU] map_init: can not map");
//  printf("[MMU] map the page pool status %lX\n", status);

 // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  status = mmu_map_pages(g_kernel_pgt_base, TRAMPOLINE, PAGESIZE, (uint64)trampoline, PTE_R | PTE_X);
  if (status)
      panic("[MMU] map_init: can not map");

}

void mm_init(void) {

  printf("[MMU] Memory map: Kernel start = 0x%lX\n", KSTART);  
  printf("[MMU] Memory map: BSS: 0x%lX -> 0x%lX\n", BSS_START, BSS_END);
//  printf("[MMU] Memory map: PG Table: 0x%lX -> 0x%X\n", _pgtable_start, _pgtable_end);
  printf("[MMU] Memory map: Stack top: 0x%lX\n", STACK_TOP);

  mem_start = (uintptr_t)(PGROUNDUP((uint64)STACK_TOP) + 0x1000 + 0x1000 * CONFIG_MP_NUM_CPUS+1);
  mem_end = (uintptr_t)(JH7110_DDR_BASE + JH7110_DDR_SIZE) ;

  mem_end = 0x81000000;  // limit for debuging

  printf("[MMU] Memory map: Free memory: 0x%lX -> 0x%lX\n", mem_start, mem_end);

  pg_pool_init((void *)mem_start, (void *)mem_end);

  kernel_mapping();

  printf("[MMU] mmu_init: Init tasks stacks ... ");
  sched_map_stacks(g_kernel_pgt_base);
  printf("Done.\n");

  printf("[MMU] mmu_enable: satp=%lX\n", g_kernel_pgt_base);
  mmu_enable(g_kernel_pgt_base, 0);
  mmu_invalidate_tlbs();
  printf("[MMU] init is Done\n");
}