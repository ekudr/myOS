#include <common.h>
#include <mmc.h>
#include <part.h>

extern disk_t boot_disk;


// we use one MMC device for SD card
// drivers from U-boot

mmc_t sd_mmc0;
mmc_config_t mmc_cfg0;


int mmc_dev_init(void) {
    int err;
    mmc_t *mmc;

    boot_disk.dev_inited = false;
    mmc = &sd_mmc0;
    mmc->cfg = &mmc_cfg0;

//    debug("[SD_CARD] Allocated mmc_t 0x%lX mmc_cfg_t 0x%lX\n", mmc, mmc->cfg);

    err = board_init_mmc(mmc);
    if(err)
        return err;

    debug("[SD_CARD] MMC OPS send_cmd 0x%lX set_ios 0x%lX\n", mmc->cfg->ops->send_cmd, mmc->cfg->ops->set_ios);    

    err = mmc_init(mmc);
    if(err) 
        return err;

    // set boot disk    
	boot_disk.mmc = mmc;
	boot_disk.dev_inited = true;
    debug("[SD_CARD] Inited \n");
    return 0;
}