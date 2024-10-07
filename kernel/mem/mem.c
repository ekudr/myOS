#include <common.h>
#include <mmu.h>
#include <spinlock.h>


#define KMEM_HEAP_BASE  0x0A00000000UL
#define KMEM_HEAP_SIZE  0x200000UL

struct mem_chunk {
    struct mem_chunk *next; //flink
    struct mem_chunk *prev; //blink
    uint64_t  size;
} _ALIGN(32);
/*
struct alloc_chunk {
    struct alloc_chunk *flink;
    struct alloc_chunk *blink;
    uint64  size;
} _ALIGN(32);
*/
struct chunk_list {
    struct mem_chunk *head;
    struct mem_chunk *tail;
};

struct kmem_heap{
  struct spinlock lock;
  struct chunk_list free;
  struct chunk_list alloc;
  uint64_t n_alloc_chunks;
  uint64_t n_free_chunks;
} _ALIGN(32);


struct kmem_heap kmem;

static inline void 
chunk_addlist(struct chunk_list *list, struct mem_chunk *new)
{
    if(list->head == NULL) {
        list->head = new;
        list->tail = new;
    } else {
        list->tail->next = new;
        new->prev = list->tail;
        list->tail = new;
    }
}

static inline void 
chunk_addbefore(struct chunk_list *list, struct mem_chunk *next, struct mem_chunk *new)
{
    if(next->prev == NULL) {
        next->prev = new;
        new->next = next;
        list->head = new;
    } else {
        next->prev->next = new;
        new->prev = next->prev;
        new->next = next;
        next->prev = new;
    }
}

static inline void 
chunk_addafter(struct chunk_list *list, struct mem_chunk *prev, struct mem_chunk *new)
{
    if(prev->next == NULL) {
        prev->next = new;
        new->prev = prev;
        list->tail = new;
    } else {
        new->next = prev->next;
        new->prev = prev;
        prev->next->prev = new;
        prev->next = new;
    }
}

static inline void 
chunk_remlist(struct chunk_list *list, struct mem_chunk *chunk)
{
    if(list->head == chunk && list->tail == chunk){
        list->head = NULL;
        list->tail = NULL;
    } else if(list->head == chunk) {
        list->head = chunk->next;
        list->head->prev = NULL;
    } else if(list->tail == chunk) {
        list->tail = chunk->prev;
        list->tail->next = NULL;
    } else {
        chunk->prev->next = chunk->next;
        chunk->next->prev = chunk->prev;
    }
}

/*
 * Initialization kernel memory heap
 */
void 
kmem_init(void)
{
    void *mem_start;
    void *mem;
    uint64_t va;

    struct mem_chunk *fr;    
    debug("[MEM] struct sizes: kmem 0x%lX mem_chunk 0x%lX\n",sizeof(struct kmem_heap), sizeof(struct mem_chunk));

    for(va = KMEM_HEAP_BASE; va < KMEM_HEAP_BASE + KMEM_HEAP_SIZE; va += PAGESIZE){
        mem = pg_alloc();
        if(mem == 0){
            panic("[MEM] kmem init");
        }
        memset(mem, 0, PAGESIZE);
    
        if(mmu_map_pages(g_kernel_pgt_base, va, PAGESIZE, (uint64_t)mem, PTE_R| PTE_W) != 0){
            pg_free(mem);
            panic("[MEM] kmem init");
        }
    }

//    kmem = (struct kmem_heap*)KMEM_HEAP_BASE;
    mem_start = (void *)KMEM_HEAP_BASE;
    initlock(&kmem.lock, "kmem");

    printf("[KMEM] heap 0x%lX => 0x%lX\n", mem_start, mem_start + KMEM_HEAP_SIZE);

//    fr = (struct free_chunk*) ((uint64)kmem + sizeof(struct kmem_heap));
    fr = (struct mem_chunk*) ((uint64)mem_start);
    
    fr->next = NULL;
    fr->prev = NULL;
    fr->size = KMEM_HEAP_SIZE - sizeof(struct mem_chunk);
    debug("[KMEM] free chunk 0x%lX size 0x%lX\n", fr, fr->size);
    chunk_addlist(&kmem.free, fr);
    kmem.alloc.head = NULL;
    kmem.alloc.tail = NULL;
    kmem.n_alloc_chunks = 0;
    kmem.n_free_chunks = 1;
}

void*
kmem_alloc(uint64_t size) 
{

    struct mem_chunk *fr, *new_fr, *al;
    uint64_t sz;  

    if(!kmem.n_free_chunks) panic("no free chunks");

 
    acquire(&kmem.lock);
    for(fr = kmem.free.head; fr != NULL; fr = fr->next){
        if(fr->size >= size) break;
    }

    if(!fr) panic("free chunk");
//  printf("[MEM] kmem_alloc fr 0x%lX size 0x%lX\n", fr, fr->size);
    
    while(fr->size/2 > (size + sizeof(struct mem_chunk))){
        fr->size = (fr->size-sizeof(struct mem_chunk))/2;
        new_fr = (struct mem_chunk*)((uint64_t)fr + sizeof(struct mem_chunk) + fr->size);
//        printf("[MEM] kmem_alloc new_fr 0x%lX size 0x%lX\n", new_fr, fr->size);
        new_fr->size = fr->size;
        new_fr->next = NULL;
        new_fr->prev = NULL;
        chunk_addafter(&kmem.free, fr, new_fr);
        kmem.n_free_chunks++;     
    }

    sz = fr->size;
    chunk_remlist(&kmem.free, fr);
    kmem.n_free_chunks--;    

    al = fr;
    al->size = sz;
    al->next = NULL;
    al->prev = NULL;
/*
    if(kmem.alloc_queue) {
        kmem.alloc_queue->flink->blink = al;
        al->flink = kmem.alloc_queue->flink;        
        kmem.alloc_queue->flink = al;       
        al->blink = kmem.alloc_queue;
    } else {
        al->flink = al;
        al->blink = al;
    }
//printf("[MEM] kmem_alloc fr 0x%lX size 0x%lx new_fr 0x%lX size 0x%lx\n", fr, fr->size, new_fr, new_fr->size); 
    kmem.alloc_queue = al;
*/
    chunk_addlist(&kmem.alloc, al);
    kmem.n_alloc_chunks++;

    release(&kmem.lock);
    
    memset((void *)((uint64_t)al+sizeof(struct mem_chunk)),0,al->size);
//    printf("[MEM] kmem allocated 0x%lX size 0x%lx\n", al, al->size);
    return (void *)((uint64_t)al+sizeof(struct mem_chunk));
}

void kmem_free(void *ptr) {
    struct mem_chunk *al, *free_al, *fr, *new_fr;
    uint64_t size;
    
    free_al = (struct mem_chunk*)((uint64_t)ptr-sizeof(struct mem_chunk));
    size = free_al->size;
//    printf("[MEM] free chunk 0x%lX size 0x%lX\n", free_al, size);

    acquire(&kmem.lock);
    //
    // CHECK
    // Why to search? You have alraedy pointer!!!
    // Do we need to check it is in the list?
    //  
    for(al = kmem.alloc.head; al != NULL; al = al->next){
        if(al == free_al) {
            break;
        } 
    }
    if (al != free_al) {
       printf("[MEM] didn't find alloc chunk 0x%lX size 0x%lX\n", al, al->size);
       goto cancel;
    }

 //   printf("[MEM] found chunk 0x%lX size 0x%lX\n", al, al->size);
/*
    if(kmem.alloc_queue == al) {
        if(kmem.alloc_queue == al->flink){
            kmem.alloc_queue = NULL;
        } else {
            kmem.alloc_queue = al->flink;    
        }
    }
        
    if ((al->flink != al->blink) || (al->flink != al)) {
//        printf("[MEM] al chunk 0x%lX flink 0x%lX blink 0x%lX size 0x%lX\n", al, al->flink, al->blink, al->size);
//        printf("[MEM] flink al chunk 0x%lX flink 0x%lX blink 0x%lX size 0x%lX\n", al->flink, al->flink->flink, al->flink->blink, al->size);
        al->blink->flink = al->flink;
        al->flink->blink = al->blink;  
//                printf("[MEM] X flink al chunk 0x%lX flink 0x%lX blink 0x%lX size 0x%lX\n", al->flink, al->flink->flink, al->flink->blink, al->size);  
    }
*/
    chunk_remlist(&kmem.alloc, al);
    kmem.n_alloc_chunks--;

    new_fr = (struct mem_chunk*)al;
    memset((void *)al,0,al->size+sizeof(struct mem_chunk));
    new_fr->size = size;
    new_fr->next = NULL;
    new_fr->prev = NULL;

//    printf("[MEM] free chunk 0x%lX size 0x%lX\n", new_fr, new_fr->size);
/*
   if(kmem.free_queue) { 
        struct free_chunk *fr1 = kmem.free_queue;
        for(int i = 0; i < kmem.n_alloc_chunks; i++) {
            if(fr1->size > fr->size){
                fr1 = fr1->flink;
            } else {
                if(kmem.free_queue == fr1) {
                    fr->flink = kmem.free_queue;
                    fr->blink = kmem.free_queue->blink;
                    kmem.free_queue->blink->flink = fr;                          
                    kmem.free_queue->blink = fr; 
//                    kmem.free_queue = fr;                   
                } else {
                    fr->flink = fr1;
                    fr->blink = fr1->blink;
                    fr1->blink->flink = fr;                          
                    fr1->blink = fr;                       
                    break;
            }

//                fr->flink = kmem.free_queue;
//                fr->blink = kmem.free_queue->blink;
//                kmem.free_queue->blink->flink = fr;                          
//                kmem.free_queue->blink = fr;               
            }
        }

    } else {
        fr->flink = fr;
        fr->blink = fr;
        kmem.free_queue = fr;
    }
*/

    for(fr = kmem.free.head; fr != NULL; fr = fr->next){
//        debug("return chunk 0x%lX checking with 0x%lX 0x%lX\n", new_fr->size, fr->size, fr);
        if(fr->next == NULL) {
            chunk_addlist(&kmem.free, new_fr); 
            break;   
        }
        if(new_fr->size >= fr->size) {
            chunk_addafter(&kmem.free, fr, new_fr);
            break;
        }
       
    }

//    chunk_addlist(&kmem.free, new_fr);
    kmem.n_free_chunks++; 

cancel:
    release(&kmem.lock);
}

void kmem_info(void){
    printf("[MEM] kmem free chunks 0x%lX alloc chunks 0x%lX\n", kmem.n_free_chunks, kmem.n_alloc_chunks);
    struct mem_chunk *fr; 
//    if(kmem.free.head != NULL){ 
        for(fr = kmem.free.head; fr != NULL; fr = fr->next){
            printf("[MEM] free: 0x%lX next: 0x%lX prev: 0x%lX size 0x%lX\n", fr, fr->next, fr->prev, fr->size);
        }
//    } else {
//        printf("[MEM] kmem free list empty\n");
//    }
    struct mem_chunk *al; 
 //   if(kmem.alloc.head != NULL){ 
        for(al = kmem.alloc.head; al != NULL; al = al->next){ 
            printf("[MEM] alloc: 0x%lX next: 0x%lX prev: 0x%lX size 0x%lX\n", al, al->next, al->prev,al->size);
        }
//    } else {
//        printf("[MEM] kmem alloc list empty\n");
//    }

}

void *malloc(uint64 size) {
    if(size < 0x20)
        size = 0x20;
    return kmem_alloc(size);
}

void mfree(void *ptr) {
    if(ptr)
        kmem_free(ptr);
}

