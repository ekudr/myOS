#include <common.h>
#include <jh7110_memmap.h>
#include <sys/riscv.h>

#include "dwmmc.h"

dw_mmc *host;

dw_mmc mmc1;
 

static inline void writel(dw_mmc *host, int reg, u32 val){
    putreg32(val, host->ioaddr+reg);
}

static inline u32 readl(dw_mmc *host, int reg){
    getreg32((uint64_t)host->ioaddr+reg);
}

static int dw_wait_reset(dw_mmc *host, u32 value)
{
	unsigned long timeout = 1000;
	u32 ctrl;

	writel(host, DWMCI_CTRL, value);

	while (timeout--) {
		ctrl = readl(host, DWMCI_CTRL);
		if (!(ctrl & DWMCI_RESET_ALL))
			return 1;
	}
	return 0;
}

int mmc_init() {

    host = &mmc1;
    host->ioaddr = (void*)JH7110_SDIO1_BASE;

    writel(host, DWMCI_PWREN, 1);

    if (!dw_wait_reset(host, DWMCI_RESET_ALL)) {
		printf("%s[%d] Fail-reset!!\n", __func__, __LINE__);
		return -1;
	}
    
    return 0;
}