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


/* Map the whole I/O memory with vaddr = paddr mappings */

#define MMU_IO_BASE     (0x00000000)
#define MMU_IO_SIZE     (0x40000000)


/* Physical and virtual addresses to page tables (vaddr = paddr mapping) */

#define PGT_L1_PBASE    (uintptr_t)&m_l1_pgtable
#define PGT_L2_PBASE    (uintptr_t)&m_l2_pgtable
#define PGT_L3_PBASE    (uintptr_t)&m_l3_pgtable
#define PGT_L1_VBASE    PGT_L1_PBASE
#define PGT_L2_VBASE    PGT_L2_PBASE
#define PGT_L3_VBASE    PGT_L3_PBASE


#define PGT_L1_SIZE     (512)  /* Enough to map 512 GiB */
#define PGT_L2_SIZE     (512)  /* Enough to map 1 GiB */
#define PGT_L3_SIZE     (1024) /* Enough to map 4 MiB (2MiB x 2) */

#define KMM_PAGE_SIZE   RV_MMU_L3_PAGE_SIZE
#define KMM_PBASE       PGT_L3_PBASE   
#define KMM_PBASE_IDX   3   
#define KMM_SPBASE      PGT_L2_PBASE
#define KMM_SPBASE_IDX  2


struct pgalloc_slab_s
{
  sq_entry_t  *next;
  void        *memory;
};
typedef struct pgalloc_slab_s pgalloc_slab_t;


/* Kernel mappings simply here, mapping is vaddr=paddr */

static size_t         m_l1_pgtable[PGT_L1_SIZE] locate_data(".pgtables");
static size_t         m_l2_pgtable[PGT_L2_SIZE] locate_data(".pgtables");
static size_t         m_l3_pgtable[PGT_L3_SIZE] locate_data(".pgtables");

#define SLAB_COUNT      (sizeof(m_l3_pgtable) / RV_MMU_PAGE_SIZE)

/* Kernel mappings (L1 base) */

uintptr_t               g_kernel_mappings  = PGT_L1_VBASE;
uintptr_t               g_kernel_pgt_pbase = PGT_L1_PBASE;

uintptr_t   mem_start;
uintptr_t   mem_end;


/* L3 page table allocator */

static sq_queue_t       g_free_slabs;
static pgalloc_slab_t   g_slabs[SLAB_COUNT];




/****************************************************************************
 * Name: slab_init
 *
 * Description:
 *   Initialize slab allocator for L2 or L3 page table entries
 *
 * L2 Page table is used for SV32. L3 used for SV39
 *
 * Input Parameters:
 *   start - Beginning of the L2 or L3 page table pool
 *
 ****************************************************************************/

static void slab_init(uintptr_t start)
{
  int i;

  sq_init(&g_free_slabs);

  for (i = 0; i < SLAB_COUNT; i++)
    {
      g_slabs[i].memory = (void *)start;
      sq_addlast((sq_entry_t *)&g_slabs[i], (sq_queue_t *)&g_free_slabs);
      start += RV_MMU_PAGE_SIZE;
    }
}

/****************************************************************************
 * Name: slab_alloc
 *
 * Description:
 *   Allocate single slab for L2/L3 page table entry
 *
 * L2 Page table is used for SV32. L3 used for SV39
 *
 ****************************************************************************/

static uintptr_t slab_alloc(void)
{
  pgalloc_slab_t *slab = (pgalloc_slab_t *)sq_remfirst(&g_free_slabs);
  return slab ? (uintptr_t)slab->memory : 0;
}

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
                                               KMM_SPBASE, vaddr));                                               
      if (!pbase)
        {
          /* No, allocate 1 page, this must not fail */

          pbase = slab_alloc();
             
//          DEBUGASSERT(pbase);

          /* Map it to the new table */

          mmu_ln_setentry(KMM_SPBASE_IDX, KMM_SPBASE, pbase, vaddr,
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

  /* Initialize slab allocator for the L2/L3 page tables */

  slab_init(KMM_PBASE);
  printf("[MMU] Kernel memory tables base address 0x%lX\n", KMM_PBASE);

    /* Map I/O region, use enough large page tables for the IO region. */

  printf("[MMU] map I/O regions\n");

  mmu_ln_map_region(1, PGT_L1_VBASE, MMU_IO_BASE, MMU_IO_BASE,
                    MMU_IO_SIZE, MMU_IO_FLAGS);

  /* Map the kernel text and data for L2/L3 */


  printf("[MMU] map kernel\n");
  map_region(KSRAM_START, KSRAM_START, KSRAM_SIZE, MMU_KDATA_FLAGS);

  /* Connect the L1 and L2 page tables for the kernel text and data */

  printf("[MMU] connect the L1 and L2 page tables\n");
  mmu_ln_setentry(1, PGT_L1_VBASE, PGT_L2_PBASE, KSRAM_START, PTE_G);

  /* Map the page pool */

  printf("[MMU] map the page pool\n");
  mmu_ln_map_region(2, PGT_L2_VBASE, PGPOOL_START, PGPOOL_START, PGPOOL_SIZE,
                    MMU_KDATA_FLAGS);
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

  pg_pool_init((mem_start, mem_end);

  printf("[MMU] Memory map: first Free memory page: 0x%lX\n", pg_pool.free);

  kernel_mapping();

  printf("[MMU] mmu_enable: satp=%lX\n", g_kernel_pgt_pbase);
  mmu_enable(g_kernel_pgt_pbase, 0);
  printf("[MMU] init is Done\n");
}