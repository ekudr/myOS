#ifndef _TRAP_H
#define _TRAP_H

void usertrapret(void);


void sbi_set_timer(uint64_t stime_value);

// ask the PLIC what interrupt we should serve.
int plic_claim(void);

// tell the PLIC we've served this IRQ.
void plic_complete(int irq);

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int devintr(void);

// handle a uart interrupt, raised because input has
// arrived, or the uart is ready for more output, or
// both. called from devintr().
void uart_intr(void);

#endif /* _TRAP_H */