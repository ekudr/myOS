#ifndef __KMEM_H__
#define __KMEM_H__

#include <stdint.h>
#include <list.h>

/*
 * KSTACK stractures
 */
struct kstack
{
    uintptr_t start;
    uint64_t size;
    list_head_t list_node;
};

typedef struct kstack kstack_t;

struct kstack_manager
{
    spinlock_t lock;
    uintptr_t va_start;
    uintptr_t va_end;
    uintptr_t va_next;
    list_head_t free;
    list_head_t alloc;
};

typedef struct kstack_manager kstack_manager_t;

kstack_t *kstack_alloc(void);
int kstack_free(kstack_t *ks);

#endif /* __KMEM_H__ */