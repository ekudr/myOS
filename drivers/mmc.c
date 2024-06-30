#include <common.h>
#include <jh7110_memmap.h>
#include <sys/riscv.h>

#include "dwmmc.h"

dw_mmc *host;

dw_mmc mmc1;
 

static inline void writel(dw_mmc *host, int reg, u32 val){
    putreg32(val, (uint64_t)host->ioaddr+reg);
}

static inline u32 readl(dw_mmc *host, int reg){
    return getreg32((uint64_t)host->ioaddr+reg);
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
    host->fifo_mode = 1;

    writel(host, DWMCI_PWREN, 1);

    if (!dw_wait_reset(host, DWMCI_RESET_ALL)) {
		printf("%s[%d] Fail-reset!!\n", __func__, __LINE__);
		return -1;
	}

	host->flags = 1 << DW_MMC_CARD_NEED_INIT;

	/* Enumerate at 400KHz */
//	dwmci_setup_bus(host, mmc->cfg->f_min);

	writel(host, DWMCI_RINTSTS, 0xFFFFFFFF);
	writel(host, DWMCI_INTMASK, 0);

	writel(host, DWMCI_TMOUT, 0xFFFFFFFF);

	writel(host, DWMCI_IDINTEN, 0);
	writel(host, DWMCI_BMOD, 1);

	if (!host->fifoth_val) {
		uint32_t fifo_size;

		fifo_size = readl(host, DWMCI_FIFOTH);
		fifo_size = ((fifo_size & RX_WMARK_MASK) >> RX_WMARK_SHIFT) + 1;
		host->fifoth_val = MSIZE(0x2) | RX_WMARK(fifo_size / 2 - 1) |
				TX_WMARK(fifo_size / 2);
	}
	writel(host, DWMCI_FIFOTH, host->fifoth_val);

	writel(host, DWMCI_CLKENA, 0);
	writel(host, DWMCI_CLKSRC, 0);

	if (!host->fifo_mode)
		writel(host, DWMCI_IDINTEN, DWMCI_IDINTEN_MASK);

    return 0;
}