#include <common.h>
#include <sbi_ecall_interface.h>
#include <sys/riscv.h>
#include <mmu.h> 
#include <sched.h>

void _hart_start();
int sbi_hsm_hart_start(unsigned long hartid, unsigned long saddr, unsigned long priv);
int boot_disk_init(void);

int kernel_init(void) {

    boot_disk_init();

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
}