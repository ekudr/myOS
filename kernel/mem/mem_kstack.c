#include <common.h>
#include <spinlock.h>
#include <mmu.h>
#include <trap.h>
#include <kmem.h>
#include <list.h>


kstack_manager_t g_kstack_manager;
kstack_manager_t *gp_ksm;

int
kstack_init(void)
{
    gp_ksm = &g_kstack_manager;
    initlock(&gp_ksm->lock, "kstack");
    gp_ksm->va_end = TRAMPOLINE - 2*PAGESIZE;
    gp_ksm->va_start = TRAMPOLINE - 0x100000000;
    gp_ksm->va_next = gp_ksm->va_end;
    list_init(&gp_ksm->free);
    list_init(&gp_ksm->alloc);
    printf("[KSTACK] Virtual addresses for KSTACK 0x%lX -> 0x%lX\n", gp_ksm->va_start, gp_ksm->va_end);
    return 0;
}

kstack_t*
kstack_alloc(void)
{
    kstack_t *ks;
    uintptr_t va, next;
    uintptr_t pg;
//    int err;

    acquire(&gp_ksm->lock);
    if(!list_is_empty(&gp_ksm->free)){
        ks = list_entry(gp_ksm->free.next, kstack_t, list_node);
        list_del(&ks->list_node);
        release(&gp_ksm->lock);
        return ks;
    }

    next = gp_ksm->va_next - 2*PAGESIZE;

    if(next <= gp_ksm->va_start) {
        panic("[KSTACK] out of virtual address address");
    }

    // Allocate 2 pages for stack
    for(va = next; va < next + 2*PAGESIZE; va += PAGESIZE){
        pg = (uintptr_t)pg_alloc();
        if(pg == 0){
            panic("[KSTACK] can't allocate pg_alloc");
        }
        memset((void *)pg, 0, PAGESIZE);
    
        if(mmu_map_pages(g_kernel_pgt_base, va, PAGESIZE, (uint64_t)pg, PTE_R| PTE_W) != 0){
            pg_free((void *)pg);
            panic("[KSTACK] kstack_alloc can not map stack");
        }
    }

    ks = (kstack_t *)next;
    gp_ksm->va_next = next - PAGESIZE;

    ks->size = 2*PAGESIZE - sizeof(kstack_t);
    ks->start = (uintptr_t)ks + sizeof(kstack_t);
    ks->list_node.next = NULL;
    ks->list_node.prev = NULL;

    list_add(&gp_ksm->alloc, &ks->list_node);
    release(&gp_ksm->lock);
    memset((void *)ks->start, 0, ks->size);
    return ks;
}

int 
kstack_free(kstack_t *ks)
{
    memset((void *)ks->start, 0, ks->size);
    debug("[KSTACK] free stack 0x%lX start 0x%lX size 0x%lX\n", ks, ks->start, ks->size);
    acquire(&gp_ksm->lock);
    list_del(&ks->list_node);
    list_add(&gp_ksm->free, &ks->list_node);
    release(&gp_ksm->lock);
    return 0;
}
/*
void kstack_test(void){
    kstack_t *s1, *s2, *s3;
    s1 = kstack_alloc();
    s2 = kstack_alloc();
    printf("[KSTACK] Allocated s1 0x%lX start 0x%lX size 0x%lX\n", s1, s1->start, s1->size);
    printf("[KSTACK] Allocated s2 0x%lX start 0x%lX size 0x%lX\n", s2, s2->start, s2->size);
    kstack_free(s1);
    printf("alloc stacks %d\n",list_count_nodes(&gp_ksm->alloc));
    printf("free stacks %d\n",list_count_nodes(&gp_ksm->free));
    s3 = kstack_alloc();
    printf("[KSTACK] Allocated s3 0x%lX start 0x%lX size 0x%lX\n", s3, s3->start, s3->size);
}
*/