#ifndef _MMU_H
#define _MMU_H

#include <rv_mmu.h>

#define PAGESIZE    RV_MMU_PAGE_SIZE

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

extern pagetable_t  g_kernel_pgt_base;

void * pg_alloc(void);
void pg_free(void *pa);
void pg_free_range(void *pa_start, void *pa_end);
void pg_pool_init(void *start, void *end);

pagetable_t mmu_user_pt_create(void);
int mmu_map_pages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm);
void mmu_user_vmfirst(pagetable_t pagetable, uint64_t *src, uint64_t sz);
void mmu_user_pg_free(pagetable_t pagetable, uint64 sz);
void mmu_user_unmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free);

#endif /* _MMU_H */