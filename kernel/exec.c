#include <common.h>
#include <sched.h>
#include <elf.h>

int flags2perm(int flags)
{
    int perm = 0;
    if(flags & 0x1)
      perm = PTE_X;
    if(flags & 0x2)
      perm |= PTE_W;
    return perm;
}

// Load a program segment into pagetable at virtual address va.
// va must be page-aligned
// and the pages from va to va+sz must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, char *path, uint64 offset, uint64 sz)
{
    uint64 i, n;
    uint64 pa;
    uint64 len_read;

    debug("EXEC read %d bytes to virt addr 0x%lX:\n", sz, va);

    for(i = 0; i < sz; i += PAGESIZE){
        pa = mmu_walk_addr(pagetable, va + i);
        if(pa == 0)
            panic("loadseg: address should exist");
        if(sz - i < PAGESIZE)
            n = sz - i;
        else
            n = PAGESIZE;
        debug("    file %s offset 0x%lX phis addr 0x%lX %d bytes\n", path, offset+i, pa, n);
        ext4_read_file(path, pa, offset+i, n, &len_read);
        if(len_read != n)
            return -1;
    }
    
    return 0;
}

int 
exec(char *path, char **argv) {
    struct elfhdr *elf;
    struct proghdr *ph;
    pagetable_t pagetable, oldpagetable;
    uint64 len_read;
    uint64 argc, sz = 0, sp, stackbase;
    struct task *t = mytask();

    elf = (struct elfhdr *)malloc(sizeof(struct elfhdr));
    ph = (struct elfhdr *)malloc(sizeof(struct proghdr));

    ext4_read_file(path, elf, 0, sizeof(struct elfhdr), &len_read);

    if(len_read != sizeof(struct elfhdr))
        goto err_exit;

    if(elf->magic != ELF_MAGIC)
        goto err_exit;

    if((pagetable = task_pagetable(t)) == 0)
        goto err_exit;

    debug("ELF header: entry point 0x%lX, PH offset 0x%lX, PH num %d\n", elf->entry, elf->phoff, elf->phnum);

    // Load program into memory.
    for(int i=0, off=elf->phoff; i<elf->phnum; i++, off+=sizeof(struct proghdr)){

        ext4_read_file(path, ph, off, sizeof(struct proghdr), &len_read);
        debug(" has read %db PH header\n", len_read);
        if(len_read != sizeof(struct proghdr))
            goto err_exit;
        debug(" program type 0x%X, mem sz 0x%lX, file sz 0x%lX, Vaddr 0x%lX, Phis addr 0x%lX, file offset 0x%lX, allign 0x%lX\n ",
                 ph->type, ph->memsz, ph->filesz, ph->vaddr,ph->paddr, ph->off, ph->align);    
        if(ph->type != ELF_PROG_LOAD)
            continue;
        if(ph->memsz < ph->filesz)
            goto err_exit;
        if(ph->vaddr + ph->memsz < ph->vaddr)
            goto err_exit;
        if(ph->vaddr % PAGESIZE != 0)
            goto err_exit;
        uint64 sz1;
        if((sz1 = mmu_user_vmalloc(pagetable, sz, ph->vaddr + ph->memsz, flags2perm(ph->flags))) == 0) {
            debug("Allocated %d bytes\n");
            goto err_exit;
        }    
        sz = sz1;
        if(loadseg(pagetable, ph->vaddr, path, ph->off, ph->filesz) < 0)
            goto err_exit;
    }

    

    t = mytask();
    uint64 oldsz = t->sz;
    debug(" loaded task size %d, old size %d\n", sz, oldsz);

    // Allocate some pages at the next page boundary.
    // Make the first inaccessible as a stack guard.
    // Use the rest as the user stack.
    sz = PGROUNDUP(sz);
    uint64 sz1;
    if((sz1 = mmu_user_vmalloc(pagetable, sz, sz + (/*USERSTACK*/1+1)*PAGESIZE, PTE_W)) == 0)
        goto err_exit;
    sz = sz1;
    mmu_user_vmclear(pagetable, sz-(/*USERSTACK*/1+1)*PAGESIZE);
    sp = sz;
    stackbase = sp - /*USERSTACK*/1*PAGESIZE;

    debug(" User Stack base 0x%lX sp 0x%lX\n", stackbase, sp);

    // Commit to the user image.
    oldpagetable = t->pagetable;
    t->pagetable = pagetable;
    t->sz = sz;
    t->trapframe->epc = elf->entry;  // initial program counter = main
    t->trapframe->sp = sp; // initial stack pointer
    task_freepagetable(oldpagetable, oldsz);

    mfree(elf);
    mfree(ph);
    return 0; // check it

err_exit:
    debug("EXEC error exit\n");
    if(pagetable)
        task_freepagetable(pagetable, sz);
    mfree(elf);
    mfree(ph);
    return -1;
}