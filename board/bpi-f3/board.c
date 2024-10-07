#include <common.h>
#include <mmc.h>
#include <sdhci.h>
#include <sched.h>

int pinctrl_set(uint32_t pin, uint32_t sel);
void ns16550_uart_init(void);
void _hart_start();
int sbi_hsm_hart_start(unsigned long hartid, unsigned long saddr, unsigned long priv);

int spacemit_sdhci_init(mmc_t *mmc);
int spacemit_sdhci_init_host(mmc_t *mmc);

// set board specific settings
// Spacemit SDHCI host SD_CARD

int board_init_mmc(mmc_t *mmc) {
    sdhci_host_t *host;
    mmc_config_t *cfg;
    int err;

    err = spacemit_sdhci_init_host(mmc);
    if(err)
        return err;
    debug("[SD_INIT] Allocated mmc_t 0x%lX host_t 0x%lX mmc_cfg_t 0x%lX\n", mmc, mmc->priv, mmc->cfg);
    host = mmc->priv;

    host->ioaddr = (void*)SDIO1_BASE;
    host->quirks = SDHCI_QUIRK_WAIT_SEND_CMD;
	host->bus_width	= 4;
    host->max_clk = 50000000;
  
    host->host_caps = MMC_CAP(MMC_LEGACY) | MMC_MODE_1BIT  | MMC_CAP_CD_ACTIVE_HIGH;
    //    | MMC_CAP(SD_HS)   | MMC_MODE_4BIT;

    cfg = (mmc_config_t *)mmc->cfg;
	cfg->name = host->name;

//	cfg->f_min = 400000;
//	cfg->f_max = 50000000;

    mmc->host_caps = host->host_caps;

    /* Setup dsr related values */
	mmc->dsr_imp = 0;
	mmc->dsr = 0xffffffff;

    err = spacemit_sdhci_init(mmc);
    if(err)
        return err;

    return 0;
}

int
board_timer_init(void) {
    // set timer clock
    putreg32(0x43, TIMER1_CLK_BASE);

    putreg32(1, TIMER1_BASE);
    return 0;
}

int
board_timer_set_irq(void) {
    // enable IRQ
    putreg32(0x100000, TIMER1_BASE+0x10);
    printf("timer match 0x%X\n", getreg32(TIMER1_BASE+0x10));
    putreg32(1, TIMER1_BASE+0x60);
    printf("interrupts enable 0x%X\n", getreg32(TIMER1_BASE+0x60));
    printf("timer count 0x%X\n", getreg32(TIMER1_BASE+0x90));
    return 0;
}

int 
board_timer_stop(void) {
    putreg32(0, TIMER1_BASE+0x60);
    printf("interrupts enable 0x%X\n", getreg32(TIMER1_BASE+0x60));
    return 0;
}

int 
board_timer_reset(void) {
    putreg32(1, TIMER1_BASE+0x70);
    putreg32(1, TIMER1_BASE+0x8);
    return 0;
}

void 
board_timer_wait(){
    while(getreg32(TIMER1_BASE+0x90) < 0x100000) 
        ;
}

void 
board_uart_init(void) {
    ns16550_uart_init();
}

void led_init(void) {

    // connect led to GPIO_96
    pinctrl_set(HEARTBEAT_PIN, HEARTBEAT_SEL);
    // set GPIO_96 to OUT  
    putreg32(1,(GPIO_BASE+0x100+0x54));
//    putreg32(1,(GPIO_BASE+0x100+0x18));
}

void led_on(void) {
    putreg32(1,(GPIO_BASE+0x100+0x18));
}

void led_off(void) {
    putreg32(1,(GPIO_BASE+0x100+0x24));
}

void
led_switch(void) {
    if (getreg32(GPIO_BASE+0x100) & 1)
        putreg32(1,(GPIO_BASE+0x100+0x24));
    else
        putreg32(1,(GPIO_BASE+0x100+0x18));
}

void
board_start_harts(void) {
    for (int i = 1; i < CONFIG_MP_NUM_CPUS; i++) {
        if(i != cpuid()) {
            sbi_hsm_hart_start(i, (uint64)_hart_start, 2);
            udelay(100000);             
        }       
    }
}

int
board_init(void) {

    return 0;
}