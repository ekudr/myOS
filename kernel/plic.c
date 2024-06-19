#include <common.h>
#include <spinlock.h>
#include <sys/riscv.h>
#include <jh7110_memmap.h>
#include <sched.h>

//
// the riscv Platform Level Interrupt Controller (PLIC).
//

void plic_init(void) {
  // set desired IRQ priorities non-zero (otherwise disabled).
  *(uint32*)(JH7110_PLIC_BASE + UART0_IRQ*4) = 1;
  //*(uint32*)(PLIC + VIRTIO0_IRQ*4) = 1;

  int hart = cpuid();

//  printf("[PLIC] enable interrapts for HART 0x%X\n", hart);
  
  // set enable bits for this hart's S-mode
  // for the uart
  *(uint32*)(JH7110_PLIC_SENABLE(hart) + (4 * (UART0_IRQ / 32))) = (1 << (UART0_IRQ % 32));


  // set this hart's S-mode priority threshold to 0.
  *(uint32*)JH7110_PLIC_SPRIORITY(hart) = 0;
}

// ask the PLIC what interrupt we should serve.
int
plic_claim(void)
{
  int hart = cpuid();
  int irq = *(uint32*)JH7110_PLIC_SCLAIM(hart);
  return irq;
}

// tell the PLIC we've served this IRQ.
void
plic_complete(int irq)
{
  int hart = cpuid();
  *(uint32*)JH7110_PLIC_SCLAIM(hart) = irq;
}