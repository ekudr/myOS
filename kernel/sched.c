#include <common.h>
#include <sched.h>
#include <spinlock.h>
#include <rv_mmu.h>


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
  
  for(t = tasks; t < &tasks[CONFIG_NUM_TASKS]; t++) {
    char *pg = pg_alloc();
    if(pg == 0)
      panic("[SCHED] can't allocate pg_alloc");
    uint64_t va = KSTACK((int) (t - tasks));
    mmu_map_pages(g_kernel_pgt_base, va, (uint64_t)pg, RV_MMU_PAGE_SIZE, PTE_R | PTE_W);
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
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu* mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct task* mytask(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct task *p = c->task;
  pop_off();
  return p;
}