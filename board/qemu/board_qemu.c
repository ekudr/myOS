#include <common.h>

void _hart_start();
int sbi_hsm_hart_start(unsigned long hartid, unsigned long saddr, unsigned long priv);

int
board_init(void) {

    return 0;
}

void 
board_uart_init(void) {
    sifive_uart_init();
}

int board_mmc_init(void) {
    return -1;
}

int mmc_init(void) {
    return -1;
}

int mmc_dev_init(mmc_t *mmc) {
    return -1;
}

void
board_start_harts(void) {
    for (int i = 1; i < CONFIG_MP_NUM_CPUS+1; i++) {
        if(i != cpuid()) {
            sbi_hsm_hart_start(i, (uint64)_hart_start, 2);
            udelay(100000);             
        }       
    }
}