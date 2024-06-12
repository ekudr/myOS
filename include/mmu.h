#ifndef _MMU_H
#define _MMU_H

extern uintptr_t   g_kernel_pgt_pbase;

void * pg_alloc(void);
void pg_free(void *pa);

void pg_pool_init(void *start, void *end);

#endif /* _MMU_H */