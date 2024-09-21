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

pagetable_t  g_kernel_pgt_base;


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
pte_t *
mmu_walk(pagetable_t pagetable, uint64 va, int alloc) {
  if(va >= MAXVA)
    panic("walk");
//printf("[MMU] mmu_walk Resolving PgTable: 0x%lX vAddr: 0x%lX\n", pagetable, va);
  for(int level = 2; level > 0; level--) {
    pte_t *pte = &pagetable[PX(level, va)];
    if(*pte & PTE_VALID) {
      pagetable = (pagetable_t)PTE2PA(*pte);
    } else {
      if(!alloc || (pagetable = (pte_t*)pg_alloc()) == 0)
        return 0;
      memset((void *)pagetable, 0, PAGESIZE);
      *pte = PA2PTE(pagetable) | PTE_VALID;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
mmu_walk_addr(pagetable_t pagetable, uint64 va) {
  pte_t *pte;
  uint64 pa;

  if(va >= MAXVA)
    return 0;

  pte = mmu_walk(pagetable, va, 0);
  if(pte == 0)
    return 0;
  if((*pte & PTE_VALID) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

/*
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
*/
// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int
mmu_map_pages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm) {
  uint64 a, last;
  pte_t *pte;

  if(size == 0)
    panic("mappages: size");

  /* Test if this is a leaf PTE, if it is, set A+D even if they are not used
   * by the implementation.
   *
   * If not, clear A+D+U because the spec. says:
   * For non-leaf PTEs, the D, A, and U bits are reserved for future use and
   * must be cleared by software for forward compatibility.
   */

  if (perm & PTE_LEAF_MASK)
    {
      perm |= (PTE_A | PTE_D);
    }
  else
    {
      perm &= ~(PTE_A | PTE_D | PTE_U);
    }  

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for(;;){
    if((pte = mmu_walk(pagetable, a, 1)) == 0)
      return -1;
    if(*pte & PTE_VALID){
      printf("addr 0x%lX ", a);
      panic("mappages: remap");
    }
      
    *pte = PA2PTE(pa) | perm | PTE_VALID;
    if(a == last)
      break;
    a += PAGESIZE;
    pa += PAGESIZE;
  }
  return 0;
}

/*
// Create PTEs for virtual addresses starting at vAddr that refer to
// physical addresses starting at pAddr. vaddr and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.

int mmu_map_pages_(uintptr_t pagetable, uint64_t vaddr, uint64_t size, uint64_t paddr, uint64_t mmuflags)
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
*/

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

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
mmu_user_vmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if((va % PAGESIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PAGESIZE; a += PAGESIZE){
    if((pte = mmu_walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_VALID) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_VALID)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      pg_free((void*)pa);
    }
    *pte = 0;
  }
}



// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void 
mmu_free_walk(pagetable_t pagetable) {

  // there are 2^9 = 512 PTEs in a page table.
  for(int i = 0; i < 512; i++){
    pte_t pte = pagetable[i];
    if((pte & PTE_VALID) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);
      mmu_free_walk((pagetable_t)child);
      pagetable[i] = 0;
    } else if(pte & PTE_VALID){
      panic("freewalk: leaf");
    }
  }
  pg_free((void*)pagetable);
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void 
mmu_user_vmfirst(pagetable_t pagetable, uint64_t *src, uint64_t sz) {
  
  char *mem;
  printf("[MMU] init user first page 0x%lx\n", pagetable);
  if(sz >= PAGESIZE)
    panic("[MMU] mmu_user_uvmfirst: more than a page");
  mem = pg_alloc();
  memset(mem, 0, PAGESIZE);
  mmu_map_pages(pagetable, 0, PAGESIZE, (uint64_t)mem, PTE_W|PTE_R|PTE_X|PTE_U);

  memmove(mem, src, sz);
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
mmu_user_vmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if(newsz >= oldsz)
    return oldsz;

  if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PAGESIZE;
    mmu_user_vmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
mmu_user_vmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if(newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for(a = oldsz; a < newsz; a += PAGESIZE){
    mem = pg_alloc();
    if(mem == 0){
      mmu_user_vmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PAGESIZE);
    if(mmu_map_pages(pagetable, a, PAGESIZE, (uint64)mem, PTE_R|PTE_U|xperm) != 0){
      pg_free(mem);
      mmu_user_vmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}



// Free user memory pages,
// then free page-table pages.
void
mmu_user_pg_free(pagetable_t pagetable, uint64 sz) {
  if(sz > 0)
    mmu_user_unmap(pagetable, 0, PGROUNDUP(sz)/PAGESIZE, 1);
  mmu_free_walk(pagetable);
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void
mmu_user_unmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free) {
  uint64 a;
  pte_t *pte;

  if((va % PAGESIZE) != 0)
    panic("uvmunmap: not aligned");

  for(a = va; a < va + npages*PAGESIZE; a += PAGESIZE) {

    if((pte = mmu_walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if((*pte & PTE_VALID) == 0)
      panic("uvmunmap: not mapped");
    if(PTE_FLAGS(*pte) == PTE_VALID)
      panic("uvmunmap: not a leaf");
    if(do_free){
      uint64 pa = PTE2PA(*pte);
      pg_free((void*)pa);
    }
    *pte = 0;
  }
}

/*
*   create an empty user page table.
*   returns 0 if out of memory.
*/ 
pagetable_t 
mmu_user_pt_create() {

  pagetable_t pagetable;
  pagetable = (pagetable_t) pg_alloc();
  if(pagetable == 0)
    return 0;
  memset(pagetable, 0, PAGESIZE);
  return pagetable;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void
mmu_user_vmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  
  pte = mmu_walk(pagetable, va, 0);
  if(pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;
  pte_t *pte;

  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    if(va0 >= MAXVA)
      return -1;
    pte = mmu_walk(pagetable, va0, 0);
    if(pte == 0 || (*pte & PTE_VALID) == 0 || (*pte & PTE_U) == 0 ||
       (*pte & PTE_W) == 0)
      return -1;
    pa0 = PTE2PA(*pte);
    n = PAGESIZE - (dstva - va0);
    if(n > len)
      n = len;
    sbi_memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PAGESIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 n, va0, pa0;

  while(len > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = mmu_walk_addr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PAGESIZE - (srcva - va0);
    if(n > len)
      n = len;
    sbi_memmove(dst, (void *)(pa0 + (srcva - va0)), n);

    len -= n;
    dst += n;
    srcva = va0 + PAGESIZE;
  }
  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while(got_null == 0 && max > 0){
    va0 = PGROUNDDOWN(srcva);
    pa0 = mmu_walk_addr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    n = PAGESIZE - (srcva - va0);
    if(n > max)
      n = max;

    char *p = (char *) (pa0 + (srcva - va0));
    while(n > 0){
      if(*p == '\0'){
        *dst = '\0';
        got_null = 1;
        break;
      } else {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PAGESIZE;
  }
  if(got_null){
    return 0;
  } else {
    return -1;
  }
}