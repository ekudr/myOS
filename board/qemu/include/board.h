#ifndef __BOARD_QEMU_H__
#define __BOARD_QEMU_H__

#define __SIFIVE_U__

#define CONFIG_MP_NUM_CPUS      4   

/* Map the whole I/O memory with vaddr = paddr mappings */

#define MMU_IO_BASE     0x00000000UL
#define MMU_IO_SIZE     0x40000000UL

/* DDR start address */
#define DDR_BASE   0x80000000UL
#define DDR_SIZE   0x80000000UL

// qemu puts UART registers here in physical memory.
#define UART0 0x10010000UL
#define UART0_IRQ 4
#define UART0_REG_SHIFT 1
#define UART0_DIV 0x08

#define SDIO1_BASE 0x16020000UL // no in qemu

// core local interruptor (CLINT), which contains the timer.
#define CLINT 0x2000000L
#define CLINT_MTIMECMP(hartid) (CLINT + 0x4000 + 8*(hartid))
#define CLINT_MTIME (CLINT + 0xBFF8) // cycles since boot.


#define CONFIG_SYS_TIMER_RATE 1000000;

#define CONFIG_NUM_IRQS  53 // Num interrupts
#define NR_IRQS    CONFIG_NUM_IRQS

/* !!!!!!! Check alll !!!!!!!*/

/* PLIC Base address */
#define PLIC_BASE 0x0c000000L

/* Interrupt Priority */
#define PLIC_PRIORITY  (PLIC_BASE + 0x000000)

/* Hart 1 S-Mode Interrupt Enable */
#define PLIC_ENABLE1   (PLIC_BASE + 0x002100)
#define PLIC_ENABLE2   (PLIC_BASE + 0x002104)

/* Hart 1 S-Mode Priority Threshold */
#define PLIC_THRESHOLD (PLIC_BASE + 0x202000)

/* Hart 1 S-Mode Claim / Complete */
#define PLIC_CLAIM     (PLIC_BASE + 0x202004)

#define PLIC_SENABLE(hart) (PLIC_BASE + 0x2100 + (hart)*0x100)



#define PLIC_SPRIORITY(hart) (PLIC_THRESHOLD + (hart)*0x2000)

#define PLIC_SCLAIM(hart) (PLIC_CLAIM + (hart)*0x2000)

#endif /* __BOARD_QEMU_H__ */