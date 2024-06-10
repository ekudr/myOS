#include <common.h>
#include <rv_mmu.h>


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

#define SLAB_COUNT      (sizeof(m_l3_pgtable) / RV_MMU_PAGE_SIZE)


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

/* Kernel mappings (L1 base) */

uintptr_t               g_kernel_mappings  = PGT_L1_VBASE;
uintptr_t               g_kernel_pgt_pbase = PGT_L1_PBASE;

/* L3 page table allocator */

static sq_queue_t       g_free_slabs;
static pgalloc_slab_t   g_slabs[SLAB_COUNT];


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

  /* Initialize slab allocator for the L2/L3 page tables */

  slab_init(KMM_PBASE);
  
}

void mm_init(void) {
    kernel_mapping();

//    mmu_enable();
}