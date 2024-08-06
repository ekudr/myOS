#include <common.h>
#include <spinlock.h>
#include <sys/riscv.h>
#include <sched.h>
#include <trap.h>




/*
*   the riscv Platform Level Interrupt Controller (PLIC).
*/

static void 
__plic_toggle(uintptr_t enable_base, int irq, int enable) {
    uintptr_t reg = enable_base + (irq / 32) * 4;
    uint32_t irq_mask = 1 << (irq % 32);

    if (enable)
        putreg32(getreg32(reg) | irq_mask, reg);
    else 
        putreg32(getreg32(reg) & ~irq_mask, reg);
}

static void 
plic_toggle(int hart, int irq, int enable) {

    // !!! lock enable regs

#ifdef __SPACEMIT_K1__
    if (!enable)
        putreg32(irq, (uintptr_t)(PLIC_SCLAIM(hart)));
#endif

    __plic_toggle(PLIC_SENABLE(hart), irq, enable);

#ifdef __SPACEMIT_K1__
    if (enable)
        putreg32(irq, (uintptr_t)(PLIC_SCLAIM(hart)));
#endif

    // !!! unlock enable regs
}

void 
plic_hart_init(void) {
    int hart = cpu_get_hartid(cpuid());
#ifdef __SPACEMIT_K1__
//    if (hart == 3)
        plic_toggle(hart, TIMER1_IRQ, 1); 
#endif
    plic_toggle(hart, UART0_IRQ, 1);

    putreg32(0, PLIC_SPRIORITY(hart));

}

void plic_init(void) {

  int id;

  /* Priority 1 for all interrupts */
  for (id = 1; id <= NR_IRQS; id++)
    {
      putreg32(1, (uintptr_t)(PLIC_PRIORITY + 4 * id));
//      printf("[PLIC] 0x%lX = %d\n", (PLIC_PRIORITY + 4 * id), getreg32((uintptr_t)(PLIC_PRIORITY + 4 * id)));
    }
debug("\n[PLIC] Interrupt pending map:\n");
  
  for (id = 0; id <= NR_IRQS / 8; id+=4)
    {
      putreg32(0,(uintptr_t)(PLIC_BASE + 0x1000 + id));
      debug("[PLIC] 0x%lX\n",getreg32((uintptr_t)(PLIC_BASE + 0x1000 + id)));
    }
//  int hart = cpuid();

//  disable all interrupts for all cpus
  for (int i=0; i<CONFIG_MP_NUM_CPUS; i++){
    int hart = cpu_get_hartid(i);
    for (id = 0; id <= NR_IRQS / 8; id+=4)
    {
      putreg32(0, (uintptr_t)(PLIC_SENABLE(hart) + (4 * (id / 32))));
    }
    /*set enable bits for this hart's S-mode for the uart   */ 
//    putreg32((1 << (UART0_IRQ % 32)), (uintptr_t)(PLIC_SENABLE(hart) + (4 * (UART0_IRQ / 32))));
//    putreg32(0x800000, (uintptr_t)(PLIC_SENABLE(i) ));    
//    putreg32((1 << (11 % 32)), (uintptr_t)(PLIC_SENABLE(i) + (4 * (11 / 32))));
//    putreg32((1 << (TIMER1_IRQ % 32)), (uintptr_t)(PLIC_SENABLE(i) + (4 * (TIMER1_IRQ / 32))));
//    putreg32((1 << (23 % 32)), (uintptr_t)(PLIC_MENABLE(i) + (4 * (23 / 32))));
  }

//  debug("[PLIC] UART0 enable %dbit in address 0x%lX\n", (UART0_IRQ % 32), (PLIC_SENABLE(hart-1) + (4 * (UART0_IRQ / 32))));


for (int i=0; i<CONFIG_MP_NUM_CPUS; i++){
  int hart = cpu_get_hartid(i);
//  printf("[PLIC] Int EN HART %d 0x%lX = %lX\n",i,PLIC_MENABLE(i), getreg32(PLIC_MENABLE(i)));
  printf("[PLIC] Int EN HART %d 0x%lX = %lX\n",i,PLIC_SENABLE(hart), getreg32(PLIC_SENABLE(i)));
  
}

  // set this hart's S-mode priority threshold to 7.

for (int i=0; i<CONFIG_MP_NUM_CPUS; i++){
  putreg32(7, PLIC_SPRIORITY(cpu_get_hartid(i)));
}

putreg32(0, PLIC_SPRIORITY(0));
for (int i=0; i<CONFIG_MP_NUM_CPUS; i++){
  int hart = cpu_get_hartid(i);
  printf("[PLIC] Priority HART %d 0x%lX = %d\n",i,PLIC_SPRIORITY(hart), getreg32(PLIC_SPRIORITY(hart)));
}


}

// ask the PLIC what interrupt we should serve.
int plic_claim(void) {
  int hart = cpu_get_hartid(cpuid());
  int irq = getreg32((uintptr_t)(PLIC_SCLAIM(hart)));
  return irq;
}

// tell the PLIC we've served this IRQ.
void plic_complete(int irq) {
  int hart = cpu_get_hartid(cpuid());
  putreg32(irq, (uintptr_t)(PLIC_SCLAIM(hart)));
}

void plic_info(void) {
//  int hart = cpu_get_hartid(cpuid());
  debug("[PLIC] Interrupt pending map:\n");
    for (int id = 0; id <= NR_IRQS / 8; id+=4)
    {
      debug("[PLIC] 0x%lX\n",getreg32((uintptr_t)(PLIC_BASE + 0x1000 + id)));
    }
  for (int i=0; i<CONFIG_MP_NUM_CPUS; i++){
    int hart = cpu_get_hartid(i);
    printf("[PLIC] Int EN HART %d 0x%lX = %lX\n",hart,PLIC_SENABLE(hart), getreg32(PLIC_SENABLE(hart)));
    printf("[PLIC] Int EN HART %d 0x%lX = %lX\n",hart,PLIC_SENABLE(hart)+4, getreg32(PLIC_SENABLE(hart)+4));
    printf("[PLIC] Priority HART %d 0x%lX = %d\n",hart,PLIC_SPRIORITY(hart), getreg32(PLIC_SPRIORITY(hart)));
    printf("[PLIC] Claim HART %d 0x%lX = %d\n",hart,PLIC_SCLAIM(hart), getreg32(PLIC_SCLAIM(hart)));
  }

  
}