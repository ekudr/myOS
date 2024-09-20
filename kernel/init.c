#include <common.h>
#include <sbi_ecall_interface.h>
#include <sys/riscv.h>
#include <mmu.h> 
#include <sched.h>


int boot_disk_init(void);
int mmc_dev_init(void);
int bootfs_init(void);

int kernel_init(void) {

    console_init();

    mmc_dev_init();
    
    boot_disk_init();

    bootfs_init();

    board_start_harts();

  
	printf("[SCHED] cpu id = 0x%lX\n", cpuid()) ;
    printf("stack pointer: 0x%lX\n", r_sp());

    printf("S mode status register 0x%lX\n",r_sstatus());
    printf("S mode interrupt register 0x%lX\n",r_sie());
    printf("S interrupt pending register 0x%lX\n",r_sip());
   printf("Timer: 0x%lx\n",get_timer(0));    
    printf("[USER] init ... ");
    shed_user_init();
    printf("Done.\n");
//    while(1){
//        if(!uart_int_pending())
//                printf("p ");
//    }
//plic_info();
led_init();
led_on();
#ifdef __SPACEMIT_K1__
    board_timer_init();
    board_timer_set_irq();
#endif    
//    plic_info();
//    board_timer_wait();
//  sbi_set_timer(0x3FFFFFF);
//  plic_info();
//  sbi_hsm_info();
//  cpu_info();
//      printf("S mode status register 0x%lX\n",r_sstatus());
//    printf("S mode interrupt register 0x%lX\n",r_sie());
//    printf("S interrupt pending register 0x%lX\n",r_sip());
	scheduler();
}