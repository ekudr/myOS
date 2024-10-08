#ifndef _SHED_H
#define _SHED_H

#include <sys/types.h>
#include <spinlock.h>
#include <sys/riscv.h>
#include <mmu.h>
#include <kmem.h>


#define NTASKS CONFIG_NUM_TASKS 


// Saved registers for kernel context switches.
typedef struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
} context;



// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
typedef struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
} trapframe, trapframe_t;

// Per-CPU state.
struct cpu {
  int hartid;
  struct task *task;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[CONFIG_MP_NUM_CPUS];

enum task_state { 
    NEW,
    UNUSED, 
    USED, 
    SLEEPING, 
    RUNNABLE, 
    RUNNING, 
    ZOMBIE
};

// Per-process state
typedef struct task {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum task_state state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // wait_lock must be held when using this:
  struct task *parent;         // Parent process

  // these are private to the process, so p->lock need not be held.
  kstack_t *kstack1;
//  uint64_t kstack;               // Virtual address of kernel stack
  uint64_t sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[CONFIG_NUM_FILES];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)

  // Task list links
  struct task *next, *prev;
  
} task, task_t;

struct task_list {
    task_t *head;
    task_t *tail;
};

typedef struct task_manager {
    int nextpid;
    struct spinlock pid_lock;
    struct spinlock list_lock;

    struct task_list tasks;
    task_t *inittask;
} task_manager_t;


// Task manager
extern task_manager_t *gp_tm;

struct cpu* mycpu(void);
int cpuid(void);
struct task* mytask(void);
void swtch(context*, context*);
void sched(void);
void scheduler(void);
void wakeup(void *chan);
void yield(void);
void reparent(task_t *t); 
int fork(void);
void forkret(void);
void exit(int status);
void sched_map_stacks(pagetable_t pgt_base);
void free_task(task_t *t);
int sched_alloc_pid(void);
int sched_taskfree(task_t *t);
task_t* sched_taskalloc(void);
pagetable_t sched_task_pagetable(task_t *t);
void sched_task_freepagetable(pagetable_t pagetable, uint64_t sz);
void shed_user_init(void);
int sched_initstart(void);
int killed(task_t *t);
void setkilled(task_t *t);
void cpu_set_hartid(int cpu_id, int hartid);
int cpu_get_hartid(int cpu_id);
int exec(char *path, char **argv);



#endif /* _SHED_H */