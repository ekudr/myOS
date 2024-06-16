#ifndef _MMU_H
#define _MMU_H

extern uintptr_t   g_kernel_pgt_base;

void * pg_alloc(void);
void pg_free(void *pa);
void pg_free_range(void *pa_start, void *pa_end);

void pg_pool_init(void *start, void *end);
int mmu_map_pages(uintptr_t pagetable, uint64_t vaddr, uint64_t size, uint64_t paddr, uint64_t mmuflags);

#endif /* _MMU_H */