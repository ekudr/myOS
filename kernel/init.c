#include <common.h>
#include <sbi_ecall_interface.h>
#include <sys/riscv.h>
#include <mmu.h> 
#include <sched.h>


int boot_disk_init(void);
int mmc_dev_init(void);
int bootfs_init(void);
void console_init(void);
void board_start_harts(void);



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
//    newsched_test();
//    shed_user_init();
    sched_initstart();
    printf("Done.\n");
//    kmem_info();
//    while(1){
//        if(!uart_int_pending())
//                printf("p ");
//    }
//plic_info();

#if (defined(__JH7110__) || defined(__SPACEMIT_K1__))
    led_init();
    led_on();
#endif
#ifdef __SPACEMIT_K1__
    board_timer_init();
    board_timer_set_irq();
#endif    
//    plic_info();
//    board_timer_wait();
    sbi_set_timer(timer_get_count()+usec_to_tick(1000000));
//  plic_info();
//  sbi_hsm_info();
//  cpu_info();
//      printf("S mode status register 0x%lX\n",r_sstatus());
//    printf("S mode interrupt register 0x%lX\n",r_sie());
//    printf("S interrupt pending register 0x%lX\n",r_sip());
	scheduler();
    return 0; // never return
}