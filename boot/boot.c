#include <common.h>
#include <string.h>
#include <sbi_ecall_interface.h>
#include <sys/riscv.h>
#include <mmu.h> 
#include <sched.h>

extern char _bss_start, _bss_end;
extern char _pgtable_start, _pgtable_end;

u64 boot_cpu_hartid;
static char* version = VERSION_STR;

#define FB	(u8*)0xfe000000
#define FB_LEN	3840*2160*4

void plic_init(void); 
void tasks_init(void);
void mm_init(void);
void kernelvec();
void _hart_start();
void trap_init(void);
void sbi_init (void);
int sbi_hsm_hart_start(unsigned long hartid, unsigned long saddr, unsigned long priv);
int sd_init(void);


/*
void memset(void *b, int c, int len)
    {
       char *s = b;

        while(len--)
            *s++ = c;
    }
*/

void boot_init_hart(){

    __sync_synchronize();

    printf("hart %d starting\n", cpuid());
    printf("stack pointer: 0x%lX\n", r_sp());

    mmu_enable((uint64_t)g_kernel_pgt_base, 0);    // turn on paging
    w_stvec((uint64)kernelvec);   // install kernel trap vector
//    plicinithart();   // ask PLIC for device interrupts

  scheduler();   
}
     



int boot_start(void)
{

    /* Prepare the bss memory region */
//    memset(&_bss_start, 0, (&_bss_end - &_bss_start));


    /* Crashing the screen image. test :) */
    /*
        int x = 0;
    for(int i = 0; i < FB_LEN; i++) {
	*(FB+(i*4)) = x++;
	*(FB+(i*4)+1) = x++;
	*(FB+(i*4)+2) = x++;
	*(FB+(i*4)+3) = 0x00;
    }
    */
       
    for(int i = 0; i < FB_LEN; i++) {
	*(u8*)(FB+i) = 0x00;
    }

    *(u8*)((1920*540+1000)*4) = 0xFF;

	sbi_init();

//	uart_init();

    printf("\nmyOS version ");
    printf(version);
    printf("\n");

    printf("Timer: 0x%lx\n",timer_get_count());

    printf("Boot HART is 0x%lX\n", boot_cpu_hartid);
    printf("TP is 0x%lX\n", r_tp());

    mm_init();

    printf("[SCHED] init tasks ... ");
    tasks_init();
    printf("Done.\n");

	printf("[TRAP] init interrupts ... ");
    trap_init();
    printf("Done.\n");

	printf("[PLIC] init interrupts ... ");
	plic_init();
    w_sstatus(r_sstatus() | SSTATUS_SIE);
    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    
    printf("Done.\n");

	printf("[CONSOLE] init ... ");

	printf("Done.\n");

    printf("[SD_CARD] init ... ");
 #ifdef _SIFIVE_U_   
    sd_init();
 #endif 
#ifdef _JH7110_
    mmc_init();
#endif    
    printf("Done.\n");

    printf("Timer: 0x%lx\n",timer_get_count());
    printf("S mode status register 0x%lX\n",r_sstatus());
    printf("S mode interrupt register 0x%lX\n",r_sie());
	__sync_synchronize();

    sbi_hsm_hart_start(1, (uint64)_hart_start, 2);
    sbi_hsm_hart_start(2, (uint64)_hart_start, 2);
    sbi_hsm_hart_start(3, (uint64)_hart_start, 2);
    sbi_hsm_hart_start(4, (uint64)_hart_start, 2);

	printf("[SCHED] cpu id = 0x%lX\n", cpuid()) ;
    printf("stack pointer: 0x%lX\n", r_sp());

    printf("[USER] init ... ");
    shed_user_init();
    printf("Done.\n");


	scheduler();
    while(1){}
}