/****************************************************************************
 * arch/risc-v/src/common/riscv_mmu.c
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
#include <printf.h>
#include <sys/riscv.h>
#include <mmu.h>

uintptr_t   g_kernel_pgt_base;


static const size_t g_pgt_sizes[] =
{
    RV_MMU_L1_PAGE_SIZE, RV_MMU_L2_PAGE_SIZE, RV_MMU_L3_PAGE_SIZE
};

/* Return the address of the PTE in page table pagetable
*  that corresponds to virtual address va.  If alloc!=0,
*  create any required page-table pages.
* 
*  The risc-v Sv39 scheme has three levels of page-table
*   pages. A page-table page contains 512 64-bit PTEs.
*   A 64-bit virtual address is split into five fields:
*     39..63 -- must be zero.
*     30..38 -- 9 bits of level-2 index.
*     21..29 -- 9 bits of level-1 index.
*     12..20 -- 9 bits of level-0 index.
*      0..11 -- 12 bits of byte offset within the page.
*/ 

uintptr_t mmu_walk_tbls(uintptr_t pagetable, uintptr_t vaddr, int alloc) {

  uintptr_t lntable = pagetable;
  uintptr_t newtable;

//  printf("[MMU] mmu_walk Resolving PgTable: 0x%lX vAddr: 0x%lX\n", pagetable, vaddr);
  if(vaddr >= MAXVA)
    panic("[MMU] Scan mem tables out of range Sv39");
    
  for (int level = 1; level < 3; level++) {

    pte_t pte = mmu_ln_getentry(level, lntable, vaddr);
    
//    printf("[MMU] mmu_walk check pte = 0x%lX level: 0x%lX vAddr: 0x%lX pTable: 0x%lX\n", pte, level, vaddr, lntable);

    if ( pte & PTE_VALID ) {
        lntable = mmu_pte_to_paddr(mmu_ln_getentry(level, lntable, vaddr)); 
//        printf("[MMU] mmu_walk next PgTable: 0x%lX vAddr: 0x%lX level: 0x%lX\n", lntable, vaddr, level);      
    } else {
        if(!alloc || ( newtable = (uintptr_t)pg_alloc()) == 0)
          return 0;
        memset(newtable, 0, RV_MMU_PAGE_SIZE);
//        printf("[MMU] mmu_walk allocate new PgTable: 0x%lX\n", newtable);
        
        mmu_ln_setentry(level, lntable, newtable, vaddr, PTE_G);
//        printf("[MMU] mmu_walk new PgTable: 0x%lX old pte: 0x%lX\n", newtable, pte);
        lntable = newtable;
      }
  }

  return lntable;
}

// Create PTEs for virtual addresses starting at vAddr that refer to
// physical addresses starting at pAddr. vaddr and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.

int mmu_map_pages(uintptr_t pagetable, uint64_t vaddr, uint64_t size, uint64_t paddr, uint64_t mmuflags)
{
  uint64_t a, last;
  uintptr_t lntable;

  if (size == 0) 
    panic("[MMU] mmu_map_pages: zero size");
  
  a = PGROUNDDOWN(vaddr);
  last = PGROUNDDOWN(vaddr + size - 1);
  
//  printf("[MMU] mmu_map_pages  PgTable: 0x%lX vAddr: 0x%lX pAddr: 0x%lX size: 0x%lX\n", pagetable, vaddr, paddr, size);

  for (;;) {
    
    lntable = mmu_walk_tbls(pagetable, a, 1);
//    if ( == 0)
//      return -1;
//    printf("[MMU] mmu_map_pages walk returned pte: 0x%lX = 0x%lX\n", pte, *pte);
    if(mmu_ln_getentry(3, lntable, a) & PTE_VALID) {
      printf("[MMU] mmu_map_pages: remap tbl: 0x%lx, vAddr: 0x%lX, pte: 0x%lx\n", lntable, a, mmu_ln_getentry(3, lntable, a));
      panic("[MMU] mmu_map_pages: remap");
    }
      

    mmu_ln_setentry(3, lntable, paddr, a, mmuflags | PTE_G);

    if (a == last)
      break;

    a += RV_MMU_PAGE_SIZE;
    paddr += RV_MMU_PAGE_SIZE;
  }
  
  return 0;
}


void mmu_ln_setentry(uint32_t ptlevel, uintptr_t lnvaddr, uintptr_t paddr,
                     uintptr_t vaddr, uint64_t mmuflags)
{
  uintptr_t *lntable = (uintptr_t *)lnvaddr;
  uint32_t   index;

  //DEBUGASSERT(ptlevel > 0 && ptlevel <= RV_MMU_PT_LEVELS);


  /* Test if this is a leaf PTE, if it is, set A+D even if they are not used
   * by the implementation.
   *
   * If not, clear A+D+U because the spec. says:
   * For non-leaf PTEs, the D, A, and U bits are reserved for future use and
   * must be cleared by software for forward compatibility.
   */

  if (mmuflags & PTE_LEAF_MASK)
    {
      mmuflags |= (PTE_A | PTE_D);
    }
  else
    {
      mmuflags &= ~(PTE_A | PTE_D | PTE_U);
    }

  /* Make sure the entry is valid */

  mmuflags |= PTE_VALID;

  /* Calculate index for lntable */

  index = (vaddr >> RV_MMU_VADDR_SHIFT(ptlevel)) & RV_MMU_VPN_MASK;

  /* Move PPN to correct position */

  paddr >>= RV_MMU_PTE_PPN_SHIFT;

  /* Save it */

  lntable[index] = (paddr | mmuflags);

  /* Update with memory by flushing the cache(s) */

  mmu_invalidate_tlb_by_vaddr(vaddr);
}

uintptr_t mmu_ln_getentry(uint32_t ptlevel, uintptr_t lnvaddr,
                          uintptr_t vaddr)
{
  uintptr_t *lntable = (uintptr_t *)lnvaddr;
  uint32_t  index;

// DEBUGASSERT(ptlevel > 0 && ptlevel <= RV_MMU_PT_LEVELS);

  index = (vaddr >> RV_MMU_VADDR_SHIFT(ptlevel)) & RV_MMU_VPN_MASK;

  return lntable[index];
}

void mmu_ln_restore(uint32_t ptlevel, uintptr_t lnvaddr, uintptr_t vaddr,
                    uintptr_t entry)
{
  uintptr_t *lntable = (uintptr_t *)lnvaddr;
  uint32_t  index;

//  DEBUGASSERT(ptlevel > 0 && ptlevel <= RV_MMU_PT_LEVELS);

  index = (vaddr >> RV_MMU_VADDR_SHIFT(ptlevel)) & RV_MMU_VPN_MASK;

  lntable[index] = entry;

  /* Update with memory by flushing the cache(s) */

  mmu_invalidate_tlb_by_vaddr(vaddr);
}

void mmu_ln_map_region(uint32_t ptlevel, uintptr_t lnvaddr, uintptr_t paddr,
                       uintptr_t vaddr, size_t size, uint64_t mmuflags)
{
  uintptr_t end_paddr = paddr + size;
  size_t    page_size = g_pgt_sizes[ptlevel - 1];

  //DEBUGASSERT(ptlevel > 0 && ptlevel <= RV_MMU_PT_LEVELS);

  while (paddr < end_paddr)
    {
      mmu_ln_setentry(ptlevel, lnvaddr, paddr, vaddr, mmuflags);
      paddr += page_size;
      vaddr += page_size;
    }
}

size_t mmu_get_region_size(uint32_t ptlevel)
{
//  DEBUGASSERT(ptlevel > 0 && ptlevel <= RV_MMU_PT_LEVELS);

  return g_pgt_sizes[ptlevel - 1];
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void mmu_user_vmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if(sz >= PAGESIZE)
    panic("[MMU] mmu_user_uvmfirst: more than a page");
  mem = pg_alloc();
  memset(mem, 0, PAGESIZE);
  mmu_map_pages((uintptr_t)pagetable, 0, PAGESIZE, (uint64)mem, PTE_W|PTE_R|PTE_X|PTE_U);
  memmove(mem, src, sz);
}
