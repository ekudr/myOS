#include <common.h>
#include <sched.h>
#include <spinlock.h>
#include <rv_mmu.h>
#include <mmu.h>
#include <string.h>


struct cpu cpus[CONFIG_MP_NUM_CPUS];

struct task tasks[CONFIG_NUM_TASKS];

struct task *inittask;

int nextpid = 1;
struct spinlock pid_lock;

extern char trampoline[]; // trampoline.S

static void free_task(struct task *t);


// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void 
sched_map_stacks(uintptr_t pgt_base) {
  struct task *t;
  int status;
  
  for(t = tasks; t < &tasks[CONFIG_NUM_TASKS]; t++) {
    uintptr_t pg = (uintptr_t)pg_alloc();
    if(pg == 0)
      panic("[SCHED] can't allocate pg_alloc");
    uint64_t va = KSTACK((int) (t - tasks));
    status = mmu_map_pages(pgt_base, va, RV_MMU_PAGE_SIZE, (uint64_t)pg, PTE_R | PTE_W);
    if (status)
      panic("[SCHED] sched_map_stacks: can not map");
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
  for(t = tasks; t < &tasks[CONFIG_NUM_TASKS]; t++) {
      initlock(&t->lock, "task");
      t->state = UNUSED;
      t->kstack = KSTACK((int) (t - tasks));
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid(void) {
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
struct task* 
mytask(void) {
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
    
    for(t = tasks; t < &tasks[CONFIG_NUM_TASKS]; t++) {
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
  struct task *t = mytask();

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

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void) {

  static int first = 1;

  // Still holding t->lock from scheduler.
  release(&mytask()->lock);

  if (first) {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
//    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct task *t = mytask();
  
  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&t->lock);  //DOC: sleeplock1
  release(lk);

  // Go to sleep.
  t->chan = chan;
  t->state = SLEEPING;

  sched();

  // Tidy up.
  t->chan = 0;

  // Reacquire original lock.
  release(&t->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
  struct task *t;

  for(t = tasks; t < &tasks[CONFIG_NUM_TASKS]; t++) {
    if(t != mytask()){
      acquire(&t->lock);
      if(t->state == SLEEPING && t->chan == chan) {
        t->state = RUNNABLE;
      }
      release(&t->lock);
    }
  }
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t task_pagetable(struct task *t) {

  pagetable_t pagetable;

  // An empty page table.
  pagetable = mmu_user_pt_create();
  if(pagetable == 0)
    return 0;
 
  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
 
  if(mmu_map_pages(pagetable, TRAMPOLINE, PAGESIZE, (uint64)trampoline, PTE_R | PTE_X) < 0){    
    mmu_user_pg_free(pagetable, 0);    
    
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if(mmu_map_pages(pagetable, TRAPFRAME, PAGESIZE,
              (uint64)(t->trapframe), PTE_R | PTE_W) < 0){
    mmu_user_unmap(pagetable, TRAMPOLINE, 1, 0);
    mmu_user_pg_free(pagetable, 0);
    return 0;
  }

  return pagetable;
}


int alloc_pid() {

  int pid;
  
  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

/* Look in the process table for an UNUSED proc.
* If found, initialize state required to run in the kernel,
* and return with t->lock held.
* If there are no free procs, or a memory allocation fails, return 0.
*/ 
static struct task* alloc_task(void) {

  struct task *t;

  for(t = tasks; t < &tasks[CONFIG_NUM_TASKS]; t++) {
    acquire(&t->lock);    
    if(t->state == UNUSED) {
      goto found;
    } else {
      release(&t->lock);
    }
  }
  return 0;

found:

  t->pid = alloc_pid();
  t->state = USED;

  // Allocate a trapframe page.
  if((t->trapframe = (struct trapframe *)pg_alloc()) == 0){  
    free_task(t);
    release(&t->lock);
    return 0;
  }

  // An empty user page table.
  t->pagetable = task_pagetable(t);
  if(t->pagetable == 0){
    free_task(t);
    release(&t->lock);
    return 0;
  }

   // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&t->context, 0, sizeof(t->context));

  t->context.ra = (uint64)forkret;
  t->context.sp = t->kstack + PAGESIZE;

  return t;
}

/*
*   free a task structure and the data hanging from it,
*   including user pages.
*   p->lock must be held.
*/ 

static void free_task(struct task *t)
{
  if(t->trapframe)
    pg_free((void*)t->trapframe);
  t->trapframe = 0;
  if(t->pagetable)
    task_freepagetable(t->pagetable, t->sz);
  t->pagetable = 0;
  t->sz = 0;
  t->pid = 0;
  t->parent = 0;
  t->name[0] = 0;
  t->chan = 0;
  t->killed = 0;
  t->xstate = 0;
  t->state = UNUSED;
}



// Free a process's page table, and free the
// physical memory it refers to.
void
task_freepagetable(pagetable_t pagetable, uint64 sz)
{
  mmu_user_unmap(pagetable, TRAMPOLINE, 1, 0);
  mmu_user_unmap(pagetable, TRAPFRAME, 1, 0);
  mmu_user_pg_free(pagetable, sz);
}


// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
u8 initcode[] = {
  0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
  0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
  0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
  0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
  0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
  0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00
};

// Set up first user process.
void 
shed_user_init(void) {

  struct task *t;

  t = alloc_task();
  inittask = t;

  // allocate one user page and copy initcode's instructions
  // and data into it.
  mmu_user_vmfirst(t->pagetable, initcode, sizeof(initcode));
  t->sz = PAGESIZE;

  // prepare for the very first "return" from kernel to user.
  t->trapframe->epc = 0;      // user program counter
  t->trapframe->sp = PAGESIZE;  // user stack pointer

  safestrcpy(t->name, "initcode", sizeof(t->name));
//  t->cwd = namei("/");

  t->state = RUNNABLE;
//  printf("[SCHED] new task allocated pid 0x%d state %d PT 0x%lX TrpFr 0x%lX\n", inittask->pid, inittask->state, inittask->pagetable, inittask->trapframe);
//  printf("[SCHED] context ra 0x%lX sp %lX ecp 0x%lX sp 0x%lX\n", inittask->context.ra, (uint64)inittask->context.sp, inittask->trapframe->epc, inittask->trapframe->sp);

  release(&t->lock);
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
  struct task *t;

  for(t = tasks; t < &tasks[CONFIG_NUM_TASKS]; t++){
    acquire(&t->lock);
    if(t->pid == pid){
      t->killed = 1;
      if(t->state == SLEEPING){
        // Wake process from sleep().
        t->state = RUNNABLE;
      }
      release(&t->lock);
      return 0;
    }
    release(&t->lock);
  }
  return -1;
}

void setkilled(struct task *t)
{
  acquire(&t->lock);
  t->killed = 1;
  release(&t->lock);
}

int killed(struct task *t)
{
  int k;
  
  acquire(&t->lock);
  k = t->killed;
  release(&t->lock);
  return k;
}