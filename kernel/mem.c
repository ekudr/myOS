#include <common.h>
#include <mmu.h>
#include <spinlock.h>


#define KMEM_HEAP_BASE  0x0A00000000UL
#define KMEM_HEAP_SIZE  0x200000UL

struct free_chunk {
    struct free_chunk *flink;
    struct free_chunk *blink;
    uint64  size;
};

struct alloc_chunk {
    struct alloc_chunk *flink;
    struct alloc_chunk *blink;
    uint64  size;
} ;

struct kmem_heap{
  struct spinlock lock;
  struct free_chunk *free_queue;
  struct alloc_chunk *alloc_queue;
  uint64 n_alloc_chunks;
  uint64 n_free_chunks;
} ;


struct kmem_heap *kmem;

// Initiation kernel memory heap
void kmem_init(void) {

    void *mem;
    uint64 va;

    struct free_chunk *fr;
    
    printf("[MEM] struct sizes: kmem 0x%lX free_chunk 0x%lX alloc_chunk 0x%lX\n",sizeof(struct kmem_heap),
             sizeof(struct free_chunk), sizeof(struct alloc_chunk));

    for(va = KMEM_HEAP_BASE; va < KMEM_HEAP_BASE + KMEM_HEAP_SIZE; va += PAGESIZE){
        mem = pg_alloc();
        if(mem == 0){
            panic("[MEM] kmem init");
        }
        memset(mem, 0, PAGESIZE);
    
        if(mmu_map_pages(g_kernel_pgt_base, va, PAGESIZE, (uint64)mem, PTE_R| PTE_W) != 0){
            pg_free(mem);
            panic("[MEM] kmem init");
        }
    }

    kmem = (struct kmem_heap*)KMEM_HEAP_BASE;
    initlock(&kmem->lock, "kmem");

    printf("[KMEM] heap 0x%lX => 0x%lX\n", kmem, KMEM_HEAP_BASE + KMEM_HEAP_SIZE);

    fr = (struct free_chunk*) ((uint64)kmem + sizeof(struct kmem_heap));
    
    fr->flink = fr;
    fr->blink = fr;
    fr->size = KMEM_HEAP_SIZE - sizeof(struct kmem_heap) - sizeof(struct free_chunk);
    printf("[KMEM] free chunk 0x%lX size 0x%lX\n", fr, fr->size);
    kmem->free_queue = fr;
    kmem->alloc_queue = NULL;
    kmem->n_alloc_chunks = 0;
    kmem->n_free_chunks = 1;

    kmem_info();
}

void *kmem_alloc(uint64 size) {

    struct free_chunk *fr, *new_fr;  
    struct alloc_chunk *al;
    uint64 sz;  

    if(!kmem->n_free_chunks) panic("no freechunks");

 
    acquire(&kmem->lock);
    fr = kmem->free_queue;
    for(int i = 0; i < kmem->n_free_chunks; i++) {
        if(fr->size >= size) break;
        fr = fr->flink; 
    }

    if(!fr) panic("free chunk");
//  printf("[MEM] kmem_alloc fr 0x%lX size 0x%lX\n", fr, fr->size);
    
    while(fr->size/2 > (size + sizeof(struct free_chunk))){
        fr->size = (fr->size-sizeof(struct free_chunk))/2;
        new_fr = (struct free_chunk*)((uint64)fr + sizeof(struct free_chunk) + fr->size);
//        printf("[MEM] kmem_alloc new_fr 0x%lX size 0x%lX\n", new_fr, fr->size);
        new_fr->size = fr->size;
        fr->flink->blink = new_fr; 
        new_fr->flink = fr->flink;
        fr->flink = new_fr;
        new_fr->blink = fr;
        kmem->n_free_chunks++;     
   
    }

    sz = fr->size;      
    if(kmem->free_queue == fr) kmem->free_queue = fr->flink;
 
    fr->flink->blink = fr->blink;
    fr->blink->flink = fr->flink;

    al = (struct alloc_chunk*)fr;
    al->size = sz;

    if(kmem->alloc_queue) {
        kmem->alloc_queue->flink->blink = al;
        al->flink = kmem->alloc_queue->flink;        
        kmem->alloc_queue->flink = al;       
        al->blink = kmem->alloc_queue;
    } else {
        al->flink = al;
        al->blink = al;
    }
//printf("[MEM] kmem_alloc fr 0x%lX size 0x%lx new_fr 0x%lX size 0x%lx\n", fr, fr->size, new_fr, new_fr->size); 
    kmem->alloc_queue = al;
    kmem->n_alloc_chunks++;
    kmem->n_free_chunks--;
    release(&kmem->lock);
    
    memset((void *)((uint64)al+sizeof(struct alloc_chunk)),0,al->size);
//    printf("[MEM] kmem allocated 0x%lX size 0x%lx\n", al, al->size);
    return (void *)((uint64)al+sizeof(struct alloc_chunk));
}

void kmem_free(void *ptr) {
    struct alloc_chunk *al, *free_al;
    struct free_chunk *fr;
    uint64 size;
    
    free_al = (struct alloc_chunk*)((uint64)ptr-sizeof(struct alloc_chunk));
    size = free_al->size;
//    printf("[MEM] free chunk 0x%lX size 0x%lX\n", free_al, size);

    acquire(&kmem->lock);
    al = kmem->alloc_queue;  
    for(int i = 0; i < kmem->n_alloc_chunks; i++) {

        if(al == free_al) {
            break;
        } 
        al = al->flink;
    }
    if (al != free_al) {
       printf("[MEM] didn't find alloc chunk 0x%lX size 0x%lX\n", al, al->size);
       goto cancel;
    }

 //   printf("[MEM] found chunk 0x%lX size 0x%lX\n", al, al->size);

    if(kmem->alloc_queue == al) {
        if(kmem->alloc_queue == al->flink){
            kmem->alloc_queue = NULL;
        } else {
            kmem->alloc_queue = al->flink;    
        }
    }
        
    if ((al->flink != al->blink) || (al->flink != al)) {
//        printf("[MEM] al chunk 0x%lX flink 0x%lX blink 0x%lX size 0x%lX\n", al, al->flink, al->blink, al->size);
//        printf("[MEM] flink al chunk 0x%lX flink 0x%lX blink 0x%lX size 0x%lX\n", al->flink, al->flink->flink, al->flink->blink, al->size);
        al->blink->flink = al->flink;
        al->flink->blink = al->blink;  
//                printf("[MEM] X flink al chunk 0x%lX flink 0x%lX blink 0x%lX size 0x%lX\n", al->flink, al->flink->flink, al->flink->blink, al->size);  
    }

    kmem->n_alloc_chunks--;

    fr = (struct free_chunk*)al;
    memset((void *)al,0,al->size+sizeof(struct alloc_chunk));
    fr->size = size;
//    printf("[MEM] free chunk 0x%lX size 0x%lX\n", fr, fr->size);
   if(kmem->free_queue) { 
        fr->flink = kmem->free_queue;
        fr->blink = kmem->free_queue->blink;
        kmem->free_queue->blink->flink = fr;                          
        kmem->free_queue->blink = fr; 
    } else {
        fr->flink = fr;
        fr->blink = fr;
        kmem->free_queue = fr;
    }
    kmem->n_free_chunks++; 

cancel:
    release(&kmem->lock);
}

void kmem_info(void){
    printf("[MEM] kmem free chunks 0x%lX alloc chunks 0x%lX\n", kmem->n_free_chunks, kmem->n_alloc_chunks);
    struct free_chunk *fr = kmem->free_queue;  
    for(int i = 0; i < kmem->n_free_chunks; i++) {
        printf("[MEM] free: 0x%lX flink: 0x%lX blink: 0x%lX size 0x%lX\n", fr, fr->flink, fr->blink,fr->size);
        fr = fr->flink; 
    }
    struct alloc_chunk *al = kmem->alloc_queue;  
    for(int i = 0; i < kmem->n_alloc_chunks; i++) {
        printf("[MEM] alloc: 0x%lX flink: 0x%lX blink: 0x%lX size 0x%lX\n", al, al->flink, al->blink,al->size);
        al = al->flink; 
    }

}

void *malloc(uint64 size) {
    return kmem_alloc(size);
}

void mfree(void *ptr) {
    kmem_free(ptr);
}

