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



struct mem_run {
  struct mem_run *next;
};

struct {
  struct spinlock lock;
  struct mem_run *freelist;
} k_mem;

/* Init kernel free memory */
void k_mem_init()
{
  initlock(&k_mem.lock, "kmem");
  k_mem.freelist = 0;
  freerange(mem_start, mem_end);
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + RV_MMU_PAGE_SIZE <= (char*)pa_end; p += RV_MMU_PAGE_SIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  struct mem_run *r;

  if(((uint64)pa % RV_MMU_PAGE_SIZE) != 0 || (char*)pa < mem_end || (uint64)pa >= mem_end)
    printf("kfree 0x%lX ",pa);

  // Fill with junk to catch dangling refs.
  memset(pa, 1, RV_MMU_PAGE_SIZE);

  r = (struct run*)pa;

  acquire(&k_mem.lock);
  r->next = k_mem.freelist;
  k_mem.freelist = r;
  release(&k_mem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void * kalloc(void)
{
  struct mem_run *r;

  acquire(&k_mem.lock);
  r = k_mem.freelist;
  if(r)
    k_mem.freelist = r->next;
  release(&k_mem.lock);

  if(r)
    memset((char*)r, 5, RV_MMU_PAGE_SIZE); // fill with junk
  return (void*)r;
}

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

  printf("[MMU] Memory map: Free memory: 0x%lX -> 0x%lX\n", mem_start, mem_end);

    kernel_mapping();

    printf("[MMU] mmu_enable: satp=%lX\n", g_kernel_pgt_pbase);
    mmu_enable(g_kernel_pgt_pbase, 0);
    printf("[MMU] init is Done\n");
}