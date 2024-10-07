#include <common.h>
#include <sched.h>
#include <spinlock.h>
#include <rv_mmu.h>
#include <mmu.h>
#include <string.h>
#include <trap.h>


struct cpu cpus[CONFIG_MP_NUM_CPUS];

//struct task tasks[CONFIG_NUM_TASKS];

//struct task *inittask;

//int nextpid = 1;
//struct spinlock pid_lock;

extern char trampoline[]; // trampoline.S


// Task manager
task_manager_t g_taskmanager;
task_manager_t *gp_tm;

/*
 *  Add task to list
 */
int
sched_taskaddlist(task_t *task)
{
    if(gp_tm->tasks.head == NULL && gp_tm->tasks.tail == NULL) {
        panic("SHED task list not initialized");  
    }
    acquire(&gp_tm->list_lock);
        task->prev = gp_tm->tasks.tail;
        gp_tm->tasks.tail->next = task;
        gp_tm->tasks.tail = task;
    release(&gp_tm->list_lock);
    return 0;
}

/*
 *  Remove task from list
 */
int
sched_taskremlist(task_t *task)
{
    // remove from list without checkin the list
    if(task->next == NULL && task->prev == NULL) {
        debug("TASK is NOT in the list\n");
        return -1;
    }
    acquire(&gp_tm->list_lock);
    if(gp_tm->tasks.head == task){
        task->next->prev = NULL;
        gp_tm->tasks.head = task->next;
    } else if(gp_tm->tasks.tail == task){
        task->prev->next = NULL;
        gp_tm->tasks.tail = task->prev;
    } else {
        task->next->prev = task->prev;
        task->prev->next = task->next;
    }
    release(&gp_tm->list_lock);
    task->next = NULL;
    task->prev = NULL;
    return 0;
}

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void 
sched_map_stacks(pagetable_t pgt_base) {
/*  
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
*/ 
}

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

// initialize the tasks table.
void tasks_init(void)
{
//  struct task *t;
  
//  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  /*
  for(t = tasks; t < &tasks[CONFIG_NUM_TASKS]; t++) {
      initlock(&t->lock, "task");
      t->state = UNUSED;
      t->kstack = KSTACK((int) (t - tasks));
  }
*/
    // new ----------------
    gp_tm = &g_taskmanager;
    initlock(&gp_tm->pid_lock, "nextpid");
    initlock(&gp_tm->list_lock, "tasklist");
    gp_tm->nextpid = 1;
    gp_tm->tasks.head = NULL;
    gp_tm->tasks.tail = NULL;
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

void 
cpu_set_hartid(int cpu_id, int hartid) {
    cpus[cpu_id].hartid = hartid;
}

int
cpu_get_hartid(int cpu_id) {
    return cpus[cpu_id].hartid;
} 

void
cpu_info(void) {
    for (int i = 0; i < CONFIG_MP_NUM_CPUS; i++) {
        printf("[CPU] cpu id %d hart id %d\n", i, cpus[i].hartid);
    }  
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

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void
wakeup(void *chan)
{
    struct task *t;
    for(t = gp_tm->tasks.head; t != NULL; t = t->next){
        if(t != mytask()){
            acquire(&t->lock);
            if(t->state == SLEEPING && t->chan == chan) {
                t->state = RUNNABLE;
            }
            release(&t->lock);
        }
    }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void
exit(int status) {

  struct task *t = mytask();

  if(t == gp_tm->inittask)
      panic("init exiting");
/*
  // Close all open files.
  for(int fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd]){
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(t->cwd);
  end_op();
  */
  t->cwd = 0;

  acquire(&wait_lock);

  // Give any children to init.
  reparent(t);

  // Parent might be sleeping in wait().
  wakeup(t->parent);
  
  acquire(&t->lock);

  t->xstate = status;
  t->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
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
    for(t = gp_tm->tasks.head; t != NULL; t = t->next){
        acquire(&t->lock);
        if(t->state == RUNNABLE) {
            debug("    hart %d PID %d state %d\n", c->hartid, t->pid, t->state);          
          
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
/*   
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
*/     
    asm ("wfi");
  }
}



// Give up the CPU for one scheduling round.
void yield(void) {
  struct task *t = mytask();
  acquire(&t->lock);
  t->state = RUNNABLE;
  sched();
  release(&t->lock);
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
  int pid;
  struct task *nt;
  struct task *t = mytask();

  // Allocate process.
  if((nt = sched_taskalloc()) == 0){
    return -1;
  }
/*
  // Copy user memory from parent to child.
  if(uvmcopy(t->pagetable, nt->pagetable, t->sz) < 0){
    free_task(nt);
    release(&nt->lock);
    return -1;
  }
  */
  nt->sz = t->sz;

  // copy saved user registers.
  *(nt->trapframe) = *(t->trapframe);

  // Cause fork to return 0 in the child.
  nt->trapframe->a0 = 0;
/*
  // increment reference counts on open file descriptors.
  for(i = 0; i < CONFIG_NUM_FILES; i++)
    if(t->ofile[i])
      nt->ofile[i] = filedup(t->ofile[i]);
  nt->cwd = idup(t->cwd);
*/
  safestrcpy(nt->name, t->name, sizeof(t->name));

  pid = nt->pid;

  release(&nt->lock);

  acquire(&wait_lock);
  nt->parent = t;
  release(&wait_lock);

  acquire(&nt->lock);
  nt->state = RUNNABLE;
  release(&nt->lock);

  return pid;
}


// Pass t's abandoned children to init.
// Caller must hold wait_lock.
void
reparent(task_t *task) {
    task_t *t;
    for(t = gp_tm->tasks.head; t != NULL; t = t->next){
        if(t->parent == task){
            t->parent = gp_tm->inittask;
        wakeup(gp_tm->inittask);
    }
  }
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



/*
 * Create a user page table for a given process, with no user memory,
 *  but with trampoline and trapframe pages.
 */ 
pagetable_t 
sched_task_pagetable(task_t *t)
{
    pagetable_t pagetable;

    // An empty page table.
    pagetable = mmu_user_pt_create();
    if(pagetable == 0)
        return 0;
 
    // map the trampoline code (for system call return)
    // at the highest user virtual address.
    // only the supervisor uses it, on the way
    // to/from user space, so not PTE_U.
 
    if(mmu_map_pages(pagetable, TRAMPOLINE, PAGESIZE, (uint64_t)trampoline, PTE_R | PTE_X) < 0){    
        mmu_user_pg_free(pagetable, 0);    
        return 0;
    }

    // map the trapframe page just below the trampoline page, for
    // trampoline.S.
    if(mmu_map_pages(pagetable, TRAPFRAME, PAGESIZE, (uint64_t)(t->trapframe), PTE_R | PTE_W) < 0){
        mmu_user_unmap(pagetable, TRAMPOLINE, 1, 0);
        mmu_user_pg_free(pagetable, 0);
        return 0;
    }

    return pagetable;
}

/*
 * Allocating next pid
 */ 
int 
sched_alloc_pid(void) {
    int pid;
  
    acquire(&gp_tm->pid_lock);
    pid = gp_tm->nextpid++;
    release(&gp_tm->pid_lock);

    return pid;
}

/* Look in the process table for an UNUSED proc.
* If found, initialize state required to run in the kernel,
* and return with t->lock held.
* If there are no free procs, or a memory allocation fails, return 0.
*/ 
/*
task_t* alloc_task(void) {

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

  t->pid = sched_alloc_pid();
  t->state = USED;

  // Allocate a trapframe page.
  if((t->trapframe = (struct trapframe *)pg_alloc()) == 0){  
    free_task(t);
    release(&t->lock);
    return 0;
  }

  // An empty user page table.
  t->pagetable = sched_task_pagetable(t);
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
*/
/*
*  Allocating a new task
*/ 
task_t*
sched_taskalloc(void)
{
    task_t *t;

    t = malloc(sizeof(task_t));
    if(t == 0)
        panic("SHED cannot alloc task mem");
    t->pid = sched_alloc_pid();
    t->state = NEW;
    t->next = NULL;
    t->prev = NULL;

    if(sched_taskaddlist(t))
        panic("SHED cannot add task to list");

    // Allocate a trapframe page.
    if((t->trapframe = (trapframe_t *)pg_alloc()) == 0){  
        sched_taskfree(t);
        return 0;
    }

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&t->context, 0, sizeof(t->context));

    t->context.ra = (uint64)forkret;
    t->context.sp = t->kstack1->start + t->kstack1->size;  // t->kstack + PAGESIZE

    return t;
}

/*
 * Free a process's page table, and free the
 * physical memory it refers to.
*/ 
void
sched_task_freepagetable(pagetable_t pagetable, uint64_t sz) {
  mmu_user_unmap(pagetable, TRAMPOLINE, 1, 0);
  mmu_user_unmap(pagetable, TRAPFRAME, 1, 0);
  mmu_user_pg_free(pagetable, sz);
}

/*
//   free a task structure and the data hanging from it,
//   including user pages.
//   t->lock must be held.
void 
free_task(task_t *t) {
  if(t->trapframe)
    pg_free((void*)t->trapframe);
  t->trapframe = 0;
  if(t->pagetable)
    sched_task_freepagetable(t->pagetable, t->sz);
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
*/
/*
 * Free task
 */
int
sched_taskfree(task_t * t)
{
    int ret;
    if(t == NULL) {
        debug("[SCHED] %s zero pointer\n", __func__);
        return -1;
    }
    ret = sched_taskremlist(t);
    if(ret < 0){
        debug("[SCHED] %s cannot remove task 0x%lX from list\n", __func__, t);
        return -1;
    }
    if(t->trapframe)
        pg_free((void*)t->trapframe);
    if(t->pagetable)
        sched_task_freepagetable(t->pagetable, t->sz);
    if(t->kstack1)
        kstack_free(t->kstack1);
    mfree(t);
    return 0;
}


// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
    struct task *t;
    for(t = gp_tm->tasks.head; t != NULL; t = t->next){
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

void setkilled(task_t *t)
{
  acquire(&t->lock);
  t->killed = 1;
  release(&t->lock);
}

int killed(task_t *t)
{
  int k;
  
  acquire(&t->lock);
  k = t->killed;
  release(&t->lock);
  return k;
}
/*
int
list_tasks()
{
    task_t *t;
    printf("[SCHED] task list:\n");
    for(t = gp_tm->tasks.head; t != NULL; t = t->next){
        printf("    PID %d state %d\n", t->pid, t->state);
    }
    return 0;
}

void
newsched_test(void)
{
    task_t *t1, *t2, *t3;
    sched_initstart();
    t1 = sched_taskalloc();
    t2 = sched_taskalloc();
    list_tasks();
    sched_taskfree(t1);
    t3 = sched_taskalloc();
    list_tasks();
}
*/