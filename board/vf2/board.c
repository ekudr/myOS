#include <common.h>
#include <mmc.h>
#include <sdhci.h>
#include <dwmmc.h>


// set board specific settings
// Synopsys DW MMC host SD_CARD

int board_init_mmc(mmc_t *mmc) {
    dw_host_t *host;
    mmc_config_t *cfg;
    uint32_t fifo_depth;
    int err;

    err = dw_mmc_init_host(mmc);
    if(err)
        return err;

    host = mmc->priv;

    host->ioaddr = (void*)SDIO1_BASE;
	// bus frequency of mmc sdio on JH7110
    host->bus_hz = 50000000;

	
	fifo_depth = 32;
	host->fifoth_val = MSIZE(0x2) | RX_WMARK(fifo_depth / 2 - 1) | TX_WMARK(fifo_depth / 2);
	host->fifo_mode = 1;
	host->buswidth = 4;

	host->dev_index = 0;

    cfg = mmc->cfg;

	cfg->name = host->name;

	cfg->f_min = 400000;
	cfg->f_max = 50000000;
	cfg->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	cfg->host_caps = host->caps;

	if (host->buswidth == 8) {
		cfg->host_caps |= MMC_MODE_8BIT;
		cfg->host_caps &= ~MMC_MODE_4BIT;
	} else {
		cfg->host_caps |= MMC_MODE_4BIT;
		cfg->host_caps &= ~MMC_MODE_8BIT;
	}
	cfg->host_caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz | MMC_MODE_HS200;

	cfg->b_max = 1024;

	

	/* Setup dsr related values */
	mmc->dsr_imp = 0;
	mmc->dsr = 0xffffffff;

	mmc->user_speed_mode = MMC_MODES_END;

    err = dw_mmc_init(mmc);
    if(err)
        return err;

    return 0;
}

int
board_init(void) {

    return 0;
}