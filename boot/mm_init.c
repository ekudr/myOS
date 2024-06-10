

/* Map the whole I/O memory with vaddr = paddr mappings */

#define MMU_IO_BASE     (0x00000000)
#define MMU_IO_SIZE     (0x40000000)

#define PGT_L1_SIZE     (512)  /* Enough to map 512 GiB */
#define PGT_L2_SIZE     (512)  /* Enough to map 1 GiB */
#define PGT_L3_SIZE     (1024) /* Enough to map 4 MiB (2MiB x 2) */


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


void kernel_mapping(void) {

}

void mm_init(void) {
    kernel_mapping();

//    mmu_enable();
}