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
#include <rv_mmu.h>
#include <queue.h>
#include <jh7110_memmap.h>
#include <spinlock.h>
#include <mmu.h>


/* Map the whole I/O memory with vaddr = paddr mappings */

#define MMU_IO_BASE     (0x00000000)
#define MMU_IO_SIZE     (0x40000000)


/* Physical and virtual addresses to page tables (vaddr = paddr mapping) */

#define PGT_L2_PBASE    m_l2_pgtable



#define PGT_L1_SIZE     (512)  /* Enough to map 512 GiB */
#define PGT_L2_SIZE     (512)  /* Enough to map 1 GiB */
#define PGT_L3_SIZE     (1024) /* Enough to map 4 MiB (2MiB x 2) */

#define KMM_PAGE_SIZE   RV_MMU_L3_PAGE_SIZE
 
#define KMM_PBASE_IDX   3   

#define KMM_SPBASE_IDX  2


struct pgalloc_slab_s
{
  sq_entry_t  *next;
  void        *memory;
};
typedef struct pgalloc_slab_s pgalloc_slab_t;


/* Kernel mappings simply here, mapping is vaddr=paddr */


static uintptr_t        m_l2_pgtable;

#define SLAB_COUNT      (sizeof(m_l3_pgtable) / RV_MMU_PAGE_SIZE)



uintptr_t   mem_start;
uintptr_t   mem_end;




/****************************************************************************
 * Name: map_region
 *
 * Description:
 *   Map a region of physical memory to the L3 page table
 *
 * Input Parameters:
 *   paddr - Beginning of the physical address mapping
 *   vaddr - Beginning of the virtual address mapping
 *   size - Size of the region in bytes
 *   mmuflags - The MMU flags to use in the mapping
 *
 ****************************************************************************/

static void map_region(uintptr_t paddr, uintptr_t vaddr, size_t size,
                       uint32_t mmuflags)
{
  uintptr_t endaddr;
  uintptr_t pbase;
  int npages;
  int i;
  int j;

  /* How many pages */

  npages = (size + RV_MMU_PAGE_MASK) >> RV_MMU_PAGE_SHIFT;
  endaddr = vaddr + size;

  for (i = 0; i < npages; i += RV_MMU_PAGE_ENTRIES)
    {
      /* See if a mapping exists */

      pbase = mmu_pte_to_paddr(mmu_ln_getentry(KMM_SPBASE_IDX,
                                               m_l2_pgtable, vaddr));                                               
      if (!pbase)
        {
          /* No, allocate 1 page, this must not fail */

          pbase = pg_alloc();
          memset(pbase, 0, RV_MMU_PAGE_SIZE);

//          DEBUGASSERT(pbase);

          /* Map it to the new table */

          mmu_ln_setentry(KMM_SPBASE_IDX, m_l2_pgtable, pbase, vaddr,
                          MMU_UPGT_FLAGS);
        }

      /* Then add the mappings */

      for (j = 0; j < RV_MMU_PAGE_ENTRIES && vaddr < endaddr; j++)
        {
          mmu_ln_setentry(KMM_PBASE_IDX, pbase, paddr, vaddr, mmuflags);
          paddr += KMM_PAGE_SIZE;
          vaddr += KMM_PAGE_SIZE;
        }
    }
}


void kernel_mapping(void) {

  int status;
 
  /* Allocate page for L1 */
  g_kernel_pgt_base = pg_alloc();
  if (!g_kernel_gt_pbase) {
    printf("[MMU] Can NOT allocate page\n");
    while (1) {}
  }
  memset(g_kernel_pgt_base, 0, RV_MMU_PAGE_SIZE);

  /* Allocate page for L2 */
  m_l2_pgtable = pg_alloc();
  if (!m_l2_pgtable) {
    printf("[MMU] Can NOT allocate page\n");
    while (1) {}
  }
  memset(m_l2_pgtable, 0, RV_MMU_PAGE_SIZE);

    /* Map I/O region, use enough large page tables for the IO region. */

  printf("[MMU] map I/O regions\n");

  mmu_ln_map_region(1, g_kernel_pgt_base, MMU_IO_BASE, MMU_IO_BASE,
                    MMU_IO_SIZE, MMU_IO_FLAGS);

  /* Map the kernel text and data for L2/L3 */


  printf("[MMU] map kernel\n");
  map_region(KSTART, KSTART, KSRAM_SIZE, MMU_KDATA_FLAGS);


  /* Connect the L1 and L2 page tables for the kernel text and data */

  printf("[MMU] connect the L1 and L2 page tables\n");
  mmu_ln_setentry(1, g_kernel_pgt_base, m_l2_pgtable, KSTART, PTE_G);

  /* Map the page pool */

  printf("[MMU] map the page pool\n");

  status = mmu_map_pages(g_kernel_pgt_base, mem_start, mem_end-mem_start, mem_start, MMU_KDATA_FLAGS);

  printf("[MMU] map the page pool status %lX\n", status);

//  mmu_ln_map_region(2, m_l2_pgtable, mem_start, mem_start, mem_end-mem_start,
//                    MMU_KDATA_FLAGS);
}

void mm_init(void) {

  printf("[MMU] Memory map: Kernel start = 0x%lX\n", KSTART);  
  printf("[MMU] Memory map: BSS: 0x%lX -> 0x%lX\n", BSS_START, BSS_END);
//  printf("[MMU] Memory map: PG Table: 0x%lX -> 0x%X\n", _pgtable_start, _pgtable_end);
  printf("[MMU] Memory map: Stack top: 0x%lX\n", STACK_TOP);

  mem_start = (uintptr_t)(PGROUNDUP((uint64)STACK_TOP) + 0x1000 + 0x1000 * CONFIG_MP_NUM_CPUS);
  mem_end = (uintptr_t)(JH7110_DDR_BASE + JH7110_DDR_SIZE) ;

  mem_end = 0x40F00000;  // limit for debuging

  printf("[MMU] Memory map: Free memory: 0x%lX -> 0x%lX\n", mem_start, mem_end);

  pg_pool_init(mem_start, mem_end);


  kernel_mapping();

  printf("[MMU] mmu_enable: satp=%lX\n", g_kernel_pgt_base);
  mmu_enable(g_kernel_pgt_base, 0);
  printf("[MMU] init is Done\n");
}