#ifndef _RV_MMU_H
#define _RV_MMU_H


/* RV32/64 page size */

#define RV_MMU_PAGE_SHIFT       (12)
#define RV_MMU_PAGE_SIZE        (1 << RV_MMU_PAGE_SHIFT) /* 4K pages */
#define RV_MMU_PAGE_MASK        (RV_MMU_PAGE_SIZE - 1)

/* Entries per PGT */

#define RV_MMU_PAGE_ENTRIES     (RV_MMU_PAGE_SIZE / sizeof(uintptr_t))

/* Common Page Table Entry (PTE) bits */

#define PTE_VALID               (1 << 0) /* PTE is valid */
#define PTE_R                   (1 << 1) /* Page is readable */
#define PTE_W                   (1 << 2) /* Page is writable */
#define PTE_X                   (1 << 3) /* Page is executable */
#define PTE_U                   (1 << 4) /* Page is a user mode page */
#define PTE_G                   (1 << 5) /* Page is a global mapping */
#define PTE_A                   (1 << 6) /* Page has been accessed */
#define PTE_D                   (1 << 7) /* Page is dirty */

/* Check if leaf PTE entry or not (if X/W/R are set it is) */

#define PTE_LEAF_MASK           (7 << 1)

/* Flags for user page tables */

#define MMU_UPGT_FLAGS          (0)

/* Flags for user FLASH (RX) and user RAM (RW) */

#define MMU_UTEXT_FLAGS         (PTE_R | PTE_X | PTE_U)
#define MMU_UDATA_FLAGS         (PTE_R | PTE_W | PTE_U)

/* I/O region flags */

#define MMU_IO_FLAGS            (PTE_R | PTE_W | PTE_G)

/* Flags for kernel page tables */

#define MMU_KPGT_FLAGS          (PTE_G)

/* Kernel FLASH and RAM are mapped globally */

#define MMU_KTEXT_FLAGS         (PTE_R | PTE_X | PTE_G)
#define MMU_KDATA_FLAGS         (PTE_R | PTE_W | PTE_G)

/* Modes, for RV32 only 1 is valid, for RV64 1-7 and 10-15 are reserved */

#define SATP_MODE_BARE          (0ul)
#define SATP_MODE_SV32          (1ul)
#define SATP_MODE_SV39          (8ul)
#define SATP_MODE_SV48          (9ul)

#define RV_MMU_PTE_PADDR_SHIFT  (10)
#define RV_MMU_PTE_PPN_SHIFT    (2)
#define RV_MMU_VADDR_SHIFT(_n)  (RV_MMU_PAGE_SHIFT + RV_MMU_VPN_WIDTH * \
                                 (RV_MMU_PT_LEVELS - (_n)))

/* Sv39 has:
 * - 4K page size
 * - 3 page table levels
 * - 9-bit VPN width
 */
#define RV_MMU_PPN_WIDTH        44
#define RV_MMU_ASID_WIDTH       16
#define RV_MMU_MODE_WIDTH       4
#define RV_MMU_PTE_PPN_MASK     (((1ul << RV_MMU_PPN_WIDTH) - 1) << RV_MMU_PTE_PADDR_SHIFT)
#define RV_MMU_VPN_WIDTH        (9)
#define RV_MMU_VPN_MASK         ((1ul << RV_MMU_VPN_WIDTH) - 1)
#define RV_MMU_PT_LEVELS        (3)
#define RV_MMU_SATP_MODE        (SATP_MODE_SV39) 
#define RV_MMU_L1_PAGE_SIZE     (0x40000000) /* 1G */
#define RV_MMU_L2_PAGE_SIZE     (0x200000)   /* 2M */
#define RV_MMU_L3_PAGE_SIZE     (0x1000)     /* 4K */

/* Minimum alignment requirement for any section of memory is 2MB */

#define RV_MMU_SECTION_ALIGN        (RV_MMU_L2_PAGE_SIZE)
#define RV_MMU_SECTION_ALIGN_MASK   (RV_MMU_SECTION_ALIGN - 1)

/* Supervisor Address Translation and Protection (satp) */

#define SATP_PPN_SHIFT          (0)
#define SATP_PPN_MASK           (((1ul << RV_MMU_PPN_WIDTH) - 1) << SATP_PPN_SHIFT)
#define SATP_ASID_SHIFT         (RV_MMU_PPN_WIDTH)
#define SATP_ASID_MASK          (((1ul << RV_MMU_ASID_WIDTH) - 1) << SATP_ASID_SHIFT)
#define SATP_MODE_SHIFT         (RV_MMU_PPN_WIDTH + RV_MMU_ASID_WIDTH)
#define SATP_MODE_MASK          (((1ul << RV_MMU_MODE_WIDTH) - 1) << SATP_MODE_SHIFT)

/* satp address to PPN translation */

#define SATP_ADDR_TO_PPN(_addr) ((_addr) >> RV_MMU_PAGE_SHIFT)
#define SATP_PPN_TO_ADDR(_ppn)  ((_ppn)  << RV_MMU_PAGE_SHIFT)


#endif /* _RV_MMU_H */