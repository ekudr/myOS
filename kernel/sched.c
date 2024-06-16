#include <common.h>
#include <sched.h>
#include <spinlock.h>
#include <rv_mmu.h>
#include <mmu.h>


struct cpu cpus[CONFIG_MP_NUM_CPUS];

struct task tasks[CONFIG_NUM_TASKS];

struct task *inittask;

int nextpid = 1;
struct spinlock pid_lock;


// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void sched_map_stacks(uintptr_t g_kernel_pgt_base)
{
  struct task *t;
  int status;
  
  for(t = tasks; t < &tasks[CONFIG_NUM_TASKS]; t++) {
    uintptr_t pg = pg_alloc();
    if(pg == 0)
      panic("[SCHED] can't allocate pg_alloc");
    uint64_t va = KSTACK((int) (t - tasks));
    status = mmu_map_pages(g_kernel_pgt_base, va, RV_MMU_PAGE_SIZE, (uint64_t)pg, PTE_R | PTE_W);
  }
}

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// initialize the tasks table.
void tasks_init(void)
{
  struct task *t;
  
  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for(t = tasks; t < &tasks[NTASKS]; t++) {
      initlock(&t->lock, "task");
      t->state = UNUSED;
      t->kstack = KSTACK((int) (t - tasks));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid() {
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu* mycpu(void) {
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct task* mytask(void) {
  push_off();
  struct cpu *c = mycpu();
  struct task *p = c->task;
  pop_off();
  return p;
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void) {
  struct task *t;
  struct cpu *c = mycpu();
  
  c->task = 0;
  for(;;){
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for(t = tasks; p < &tasks[NPROC]; t++) {
      acquire(&t->lock);
      if(t->state == RUNNABLE) {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        t->state = RUNNING;
        c->task = t;
        swtch(&c->context, &t->context);

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->task = 0;
      }
      release(&t->lock);
    }
  }
}


// Switch to scheduler.  Must hold only t->lock
// and have changed task->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be task->intena and task->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void) {
  int intena;
  struct task *t = myproc();

  if(!holding(&t->lock))
    panic("sched t->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(t->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&t->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void) {
  struct task *t = mytask();
  acquire(&t->lock);
  t->state = RUNNABLE;
  sched();
  release(&t->lock);
}