#include <common.h>
#include <spinlock.h>
#include <sys/riscv.h>
#include <jh7110_memmap.h>
#include <sched.h>
#include <trap.h>




/*
*   the riscv Platform Level Interrupt Controller (PLIC).
*/


void plic_init(void) {

  int id;

  /* Priority 1 for all interrupts */
  for (id = 1; id <= 136; id++)
    {
      putreg32(1, (uintptr_t)(JH7110_PLIC_PRIORITY + 4 * id));
    }

  int hart = cpuid();

  printf("[PLIC] enable interrapts for HART 0x%X\n", hart);
  
  /*set enable bits for this hart's S-mode for the uart   */ 
//  putreg32((1 << (UART0_IRQ % 32)), (uintptr_t)(JH7110_PLIC_SENABLE(hart) + (4 * (UART0_IRQ / 32))));
  
  putreg32(1, (uintptr_t)(JH7110_PLIC_ENABLE2));



  // set this hart's S-mode priority threshold to 0.
  *(uint32*)JH7110_PLIC_SPRIORITY(hart) = 0;

  printf("[PLIC] enable 1 register 0x%lX\n",getreg32((uintptr_t)(JH7110_PLIC_ENABLE1)));
  printf("[PLIC] enable 2 register 0x%lX\n",getreg32((uintptr_t)(JH7110_PLIC_ENABLE2)));


  //sbi_set_timer(1000000);

}

// ask the PLIC what interrupt we should serve.
int plic_claim(void) {
  int hart = cpuid();
  int irq = getreg32((uintptr_t)(JH7110_PLIC_SCLAIM(hart)));
  return irq;
}

// tell the PLIC we've served this IRQ.
void plic_complete(int irq) {
  int hart = cpuid();
  putreg32(irq, (uintptr_t)(JH7110_PLIC_SCLAIM(hart)));
}