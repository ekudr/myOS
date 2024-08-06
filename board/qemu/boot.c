#include <common.h>
#include <string.h>
#include <sbi_ecall_interface.h>
#include <sys/riscv.h>
#include <mmu.h> 
#include <sched.h>

#include <mmc.h>

#include <spi.h>
#define ACCESS(x) (*(__typeof__(*x) volatile *)(x))
extern spi_ctrl* spi;

uint64_t boot_cpu_hartid;
static char* version = VERSION_STR;

#define FB	(u8*)0xfe000000
#define FB_LEN	3840*2160*4

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

void boot_init_hart(int hart){

    cpu_set_hartid(r_tp(), hart);

    __sync_synchronize();

    printf("hart %d starting\n", cpuid());
    printf("stack pointer: 0x%lX\n", r_sp());

    mmu_enable((uint64_t)g_kernel_pgt_base, 0);    // turn on paging
    w_stvec((uint64)kernelvec);   // install kernel trap vector
//    plicinithart();   // ask PLIC for device interrupts

  scheduler();   
}
     



int boot_start(int hart)
{
    char *buf[512];
    int err;

    cpu_set_hartid(r_tp(), hart);
 	sbi_init();

    board_init();
    
    printf("\nmyOS version ");
    printf(version);
    printf("\n");

    printf("Timer: 0x%lx\n",get_timer(0));

    printf("Boot HART is 0x%lX\n", boot_cpu_hartid);
    printf("Boot HART_ID 0x%lX\n", cpu_get_hartid(r_tp()));
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
    w_sie(r_sie() | SIE_SEIE /*| SIE_STIE*/ | SIE_SSIE);
    
    printf("Done.\n");

sbi_set_timer(0x3FFFFFFF);
    printf("[SD_CARD] init ... ");
    sd_init();
    printf("Done.\n");


    kernel_init();
    //no return from kernel_init
    while(1){}
}

