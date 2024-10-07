#include <common.h>
#include <sdhci.h>
#include <mmc.h>
#include <part.h>

#define SPACEMIT_SDHC_MIN_FREQ    (400000)
#define SDHCI_CTRL_ADMA2_LEN_MODE BIT(10)
#define SDHCI_CTRL_CMD23_ENABLE BIT(11)
#define SDHCI_CTRL_HOST_VERSION_4_ENABLE BIT(12)
#define SDHCI_CTRL_ADDRESSING BIT(13)
#define SDHCI_CTRL_ASYNC_INT_ENABLE BIT(14)

#define SDHCI_CLOCK_PLL_EN BIT(3)

extern disk_t boot_disk;

// supposted we have 1 MMC dev for now

sdhci_host_t spacemit_mmc0;


static void 
sdhci_do_enable_v4_mode(sdhci_host_t *host) {
	
	uint16_t ctrl2;
	ctrl2 = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	ctrl2 |= SDHCI_CTRL_ADMA2_LEN_MODE
			| SDHCI_CTRL_CMD23_ENABLE
			| SDHCI_CTRL_HOST_VERSION_4_ENABLE
			| SDHCI_CTRL_ADDRESSING
			| SDHCI_CTRL_ASYNC_INT_ENABLE;

	sdhci_writew(host, ctrl2, SDHCI_HOST_CONTROL2);
}


int 
spacemit_sdhci_init(mmc_t *mmc){
    int ret;
    sdhci_host_t *host;
	mmc_config_t *cfg;

    host = mmc->priv;
	cfg = (mmc_config_t *)mmc->cfg;

    ret = sdhci_setup_cfg(cfg, host, host->max_clk, SPACEMIT_SDHC_MIN_FREQ);
	if (ret)
		return ret;
    debug("[SDHCI] Host configured\n");
	ret = sdhci_init(mmc);
	if (ret)
		return ret;

	/*
	enable v4 should execute after sdhci_probe, because sdhci reset would
	clear the SDHCI_HOST_CONTROL2 register.
	*/
    sdhci_do_enable_v4_mode(host);

    return 0;
}


// Init host struct
int 
spacemit_sdhci_init_host(mmc_t *mmc) {
    sdhci_host_t *host;

    char *dev_name = "Spacemit MMC SDHCI";

    host = &spacemit_mmc0;
    host->name = dev_name;
    mmc->priv = host;
   

    return 0;
}