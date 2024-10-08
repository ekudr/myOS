#include <common.h>
#include <string.h>
#include <sbi_ecall_interface.h>
#include <sys/riscv.h>
#include <mmu.h> 
#include <sched.h>


uint64_t boot_cpu_hartid;
static char* version = VERSION_STR;

//#define FB	(u8*)0xfe000000
//#define FB_LEN	3840*2160*4

int board_init(void);

void plic_init(void); 
void tasks_init(void);
void mm_init(void);
void kernelvec();
void _hart_start();
void trap_init(void);
void sbi_init (void);
int sbi_hsm_hart_start(unsigned long hartid, unsigned long saddr, unsigned long priv);
int sd_init(void);
void uart_init(void);
int boot_disk_init(void);
int kernel_init(void);
void plic_hart_init(void);


/*
void memset(void *b, int c, int len)
    {
       char *s = b;

        while(len--)
            *s++ = c;
    }
*/

void boot_init_hart(int hart){

    cpu_set_hartid(r_tp(), hart);
    printf("hart %d starting\n", cpuid());
    printf("stack pointer: 0x%lX\n", r_sp());

 //   mmu_enable((uint64_t)g_kernel_pgt_base, 0);    // turn on paging
printf("[MMU] mmu_enable: satp=%lX\n", g_kernel_pgt_base);
    // wait for any previous writes to the page table memory to finish.
  sfence_vma();

  w_satp(MAKE_SATP(g_kernel_pgt_base));

  // flush stale entries from the TLB.
  sfence_vma();

    w_stvec((uint64)kernelvec);   // install kernel trap vector
    plic_hart_init();   // ask PLIC for device interrupts

    w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);
    w_sstatus(r_sstatus() | SSTATUS_SIE);
    printf("S mode status register 0x%lX\n",r_sstatus());
    printf("S mode interrupt register 0x%lX\n",r_sie());
    printf("S interrupt pending register 0x%lX\n",r_sip());

  scheduler();   
}
     



int boot_start(int hart)
{
    cpu_set_hartid(r_tp(), hart);

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
       
    uint8_t *fb = (uint8_t *)FB;
	for (int i = 0; i < 1080; ++i) {
		for (int j = 0; j < 1920; j++) {
				*fb++ = 180;
				*fb++ = 180;
				*fb++ = 180;
				*fb++ = 0;
			}
	}

//    *(u8*)(FB+((1920*540+1000)*4)) = 0xFF;

    //    putpixel(FB, 1000, 500, 0xFF0000);
//    fillrect(FB, 200, 100, 200, 100, 0xFF);

	sbi_init();

    board_init();
    
    printf("\nmyOS version ");
    printf(version);
    printf("\n");

    printf("Timer: 0x%lx\n",get_timer(0));

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


//   for (int i = 0; i < 30; i++) {
//        printf("%d ", i);
//        udelay(1000000);
//    }

    kernel_init();

    while(1){}
}