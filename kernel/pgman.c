#include <common.h>
#include <rv_mmu.h>
#include <queue.h>
#include <jh7110_memmap.h>
#include <spinlock.h>

extern uintptr_t   mem_start, mem_end;


struct {
  struct spinlock lock;
  struct sq_entry_s *free;
} pg_pool;

/* Init free pages pool*/
void pg_pool_init(void *start, void *end)
{
    initlock(&pg_pool.lock, "pgmem");
    pg_pool.free = 0;
    pg_free_range(start, end);
    printf("[MMU] Memory map: first Free memory page: 0x%lX\n", pg_pool.free);
}

void pg_free_range(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + RV_MMU_PAGE_SIZE <= (char*)pa_end; p += RV_MMU_PAGE_SIZE)
    pg_free(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void pg_free(void *pa)
{
  struct sq_entry_s *r;

  if(((uint64)pa % RV_MMU_PAGE_SIZE) != 0 || (char*)pa < mem_end || (uint64)pa >= mem_end)

  // Fill with junk to catch dangling refs.
 // memset(pa, 1, RV_MMU_PAGE_SIZE);

  r = (struct sq_entry_s*)pa;

  acquire(&pg_pool.lock);
  r->flink = pg_pool.free;
  pg_pool.free = r;
  release(&pg_pool.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void * pg_alloc(void)
{
  struct sq_entry_s *r;

  acquire(&pg_pool.lock);
  r = pg_pool.free;
  if(r)
    pg_pool.free = r->flink;
  release(&pg_pool.lock);

  if(r)
//    memset((char*)r, 5, RV_MMU_PAGE_SIZE); // fill with junk
  return (void*)r;
}
