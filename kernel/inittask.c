#include <common.h>
#include <sched.h>
#include <trap.h>
#include <mmu.h>
#include <kmem.h>
#include <elf.h>
#include <ext4.h>

/*
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
*/

int loadseg(pagetable_t pagetable, uint64_t va, char *path, uint64_t offset, uint64_t sz);
int flags2perm(int flags);


/*
 * Create and execute first init task.
 */
int
sched_initstart(void)
{
    task_t *t;
 //   int err;
    elfhdr_t *elf;
    proghdr_t *ph;
    uint64_t sz = 0, len_read;
    char *path = "/init";

    debug("Init task:\n");
    t = malloc(sizeof(task_t));
    if(t == 0)
        panic("SHED cannot alloc task mem");
    t->pid = sched_alloc_pid();
    t->state = NEW;
    t->next = NULL;
    t->prev = NULL;

    debug("   task allocated: 0x%lX PID %d\n", t, t->pid);


    initlock(&t->lock, "task");
/*
    uintptr_t pg = (uintptr_t)pg_alloc();
    if(pg == 0)
        panic("[SCHED] can't allocate pg_alloc");
    uint64_t va = KSTACK((int)t->pid);
    err = mmu_map_pages(g_kernel_pgt_base, va, RV_MMU_PAGE_SIZE, (uint64_t)pg, PTE_R | PTE_W);
    if (err)
        panic("[SCHED] init_tack: can not map stack");
*/

    t->kstack1 = kstack_alloc();


//    t->kstack = t->kstack1->start;    

    // Allocate a trapframe page.
    if((t->trapframe = (trapframe_t *)pg_alloc()) == 0){  
        sched_taskfree(t);
        return 0;
    }

    debug("   task allocated: kstack 0x%lX -> 0x%lX trapframe 0x%lX\n", t->kstack1->start, 
                    t->kstack1->start + t->kstack1->size, t->trapframe);

    // An empty user page table.
    t->pagetable = sched_task_pagetable(t);
    if(t->pagetable == 0){
        sched_taskfree(t);
//        release(&t->lock);
        return 0;
    }

    debug("   task allocated: pagetable 0x%lX\n", t->pagetable);

    // Set up new context to start executing at forkret,
    // which returns to user space.
    memset(&t->context, 0, sizeof(t->context));

    t->context.ra = (uint64)forkret;
    t->context.sp = t->kstack1->start + t->kstack1->size;
    debug("   task allocated: task lock addr 0x%lX\n", &t->lock.locked);
    acquire(&t->lock);

    debug("Task Manager 0x%lX\n", gp_tm);
    gp_tm->inittask = t;
    gp_tm->tasks.head = t;
    gp_tm->tasks.tail = t;
    
//--------------------

    elf = (elfhdr_t *)malloc(sizeof(elfhdr_t));
    ph = (proghdr_t *)malloc(sizeof(proghdr_t));

    ext4_read_file(path, elf, 0, sizeof(elfhdr_t), &len_read);

    if(len_read != sizeof(elfhdr_t))
        panic("init load error");

    if(elf->magic != ELF_MAGIC)
        panic("init load error");
        
    // Load program into memory.
    for(int i=0, off=elf->phoff; i<elf->phnum; i++, off+=sizeof(proghdr_t)){

        ext4_read_file(path, ph, off, sizeof(proghdr_t), &len_read);
        debug(" has read %db PH header\n", len_read);
        if(len_read != sizeof(proghdr_t))
            panic("init load error");
        debug(" program type 0x%X, mem sz 0x%lX, file sz 0x%lX, Vaddr 0x%lX, Phis addr 0x%lX, file offset 0x%lX, allign 0x%lX\n ",
                 ph->type, ph->memsz, ph->filesz, ph->vaddr,ph->paddr, ph->off, ph->align);    
        if(ph->type != ELF_PROG_LOAD)
            continue;
        if(ph->memsz < ph->filesz)
            panic("init load error");
        if(ph->vaddr + ph->memsz < ph->vaddr)
            panic("init load error");
        if(ph->vaddr % PAGESIZE != 0)
            panic("init load error");
        uint64_t sz1;
        if((sz1 = mmu_user_vmalloc(t->pagetable, sz, ph->vaddr + ph->memsz, flags2perm(ph->flags))) == 0) {
            panic("init load error");
        }    
        sz = sz1;
        if(loadseg(t->pagetable, ph->vaddr, path, ph->off, ph->filesz) < 0)
            panic("init load error");
    }    
/*
    // allocate one user page and copy initcode's instructions
    // and data into it.
    mmu_user_vmfirst(t->pagetable, (uint64_t*)initcode, sizeof(initcode));
    t->sz = PAGESIZE;
*/

    // Allocate some pages at the next page boundary.
    // Make the first inaccessible as a stack guard.
    // Use the rest as the user stack.
    sz = PGROUNDUP(sz);
    uint64 sz1;
    if((sz1 = mmu_user_vmalloc(t->pagetable, sz, sz + (/*USERSTACK*/1+1)*PAGESIZE, PTE_W)) == 0)
        panic("init load error");
    sz = sz1;
    mmu_user_vmclear(t->pagetable, sz-(/*USERSTACK*/1+1)*PAGESIZE);
    
//    stackbase = sz - /*USERSTACK*/1*PAGESIZE;
 
    t->sz = sz;
    t->trapframe->epc = elf->entry;  // initial program counter = main
    t->trapframe->sp = sz; // initial stack pointer


    safestrcpy(t->name, "initcode", sizeof(t->name));
    //  t->cwd = namei("/");

    t->state = RUNNABLE;
    //  printf("[SCHED] new task allocated pid 0x%d state %d PT 0x%lX TrpFr 0x%lX\n", inittask->pid, inittask->state, inittask->pagetable, inittask->trapframe);
    //  printf("[SCHED] context ra 0x%lX sp %lX ecp 0x%lX sp 0x%lX\n", inittask->context.ra, (uint64)inittask->context.sp, inittask->trapframe->epc, inittask->trapframe->sp);

    mfree(elf);
    mfree(ph);

    release(&t->lock);


    return 0;
}