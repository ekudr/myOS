#include <common.h>
#include <sched.h>
#include <spinlock.h>


struct cpu cpus[CONFIG_MP_NUM_CPUS];

struct task tasks[CONFIG_NUM_TASKS];

struct task *inittask;

int nextpid = 1;
struct spinlock pid_lock;

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