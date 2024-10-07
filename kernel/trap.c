
#include <common.h>
#include <spinlock.h>
#include <sys/riscv.h>
#include <sched.h>
#include <trap.h>
#include <mmu.h>
#include <syscall.h>

struct spinlock tickslock;
uint64_t ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

void trap_init(void) {
  initlock(&tickslock, "time");
  w_stvec((uint64)kernelvec);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void kerneltrap() {
  
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
//  printf("trap in TP 0x%lX SSTATUS 0x%lX sepc 0x%lX\n", r_tp(), r_sstatus(), r_sepc());

  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  //if(which_dev == 2 && mytask() != 0 && mytask()->state == RUNNING)
  //  yield();


  
//  printf("0x%lX ", r_sip());  
  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  
  w_sepc(sepc);
  w_sstatus(sstatus);

//  printf("trap out TP 0x%lX SSTATUS 0x%lX sepc 0x%lX\n", r_tp(), r_sstatus(), r_sepc());
}

void
clockintr()
{
  acquire(&tickslock);
//  printf(".");
  ticks++;
//  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr(void)
{
  uint64_t scause = r_scause();
  
  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uart_intr();
    } 
#ifdef __SPACEMIT_K1__
      else if (irq == TIMER1_IRQ) {
//        printf("IRQ %d Hart %d\n", irq, cpuid());
//        uart_int_pending();
//        plic_info();
        board_timer_reset();
//        led_switch();
    } 
#endif    
      else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000005L){
    // Timer interrupt from CLINT timer interrupt
  
    if(cpuid() == 0){
      clockintr();
    }
  //  printf(".");
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
  //  w_sip(r_sip() & ~0x2);
  //sbi_set_timer(0);
    led_switch();
    sbi_set_timer(timer_get_count()+usec_to_tick(1000000));

    return 2;
  } else {
    return 0;
  }
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void usertrap(void) {

  int which_dev = 0;

  

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct task *t = mytask();
  
  // save user program counter.
  t->trapframe->epc = r_sepc();
  
  if(r_scause() == 8){
    // system call
    if(killed(t))
      exit(-1);
//    printf("[SCHED] syscall from userspace\n");
    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    t->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), t->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(t);
  }

  if(killed(t))
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct task *t = mytask();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to uservec in trampoline.S
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  
  w_stvec(trampoline_uservec);

  // set up trapframe values that uservec will need when
  // the process next traps into the kernel.
  t->trapframe->kernel_satp = r_satp();         // kernel page table
  t->trapframe->kernel_sp = t->kstack1->start + t->kstack1->size; // process's kernel stack   t->kstack + PAGESIZE
  t->trapframe->kernel_trap = (uint64)usertrap;
  t->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(t->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(t->pagetable);

  // jump to userret in trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
 
  ((void (*)(uint64))trampoline_userret)(satp);
}
