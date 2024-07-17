#include <common.h>
#include <mmc.h>
#include <sdhci.h>


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
  
    host->host_caps = MMC_CAP(MMC_LEGACY) | MMC_MODE_1BIT | MMC_MODE_4BIT
                    | MMC_CAP_CD_ACTIVE_HIGH | MMC_CAP(SD_HS);

    cfg = mmc->cfg;
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
board_init(void) {

    return 0;
}