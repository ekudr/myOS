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


 static int dw_send_cmd(/*struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data*/)
{
    int ret = 0, flags = 0, i;
	unsigned int timeout = 500;
	u32 retry = 100000;
	u32 mask, ctrl;
	u64 start = timer_get_count();
//  struct bounce_buffer bbstate;

	while (readl(host, DWMCI_STATUS) & DWMCI_BUSY) {
		if ((timer_get_count() - start) > timeout) {
			printf("%s: Timeout on data busy\n", __func__);
			return -1;
		}
	}

    writel(host, DWMCI_RINTSTS, DWMCI_INTMSK_ALL);


    return ret;
}

static int dw_setup_bus(dw_mmc *host, u32 freq)
{
	u32 div, status;
	int timeout = 10000;
	unsigned long sclk;

	if ((freq == host->clock) || (freq == 0))
		return 0;
	/*
	 * If host->get_mmc_clk isn't defined,
	 * then assume that host->bus_hz is source clock value.
	 * host->bus_hz should be set by user.
	 */
	if (host->get_mmc_clk)
		sclk = host->get_mmc_clk(host, freq);
	else if (host->bus_hz)
		sclk = host->bus_hz;
	else {
		printf("%s: Didn't get source clock value.\n", __func__);
		return -1;
	}

	if (sclk == freq)
		div = 0;	/* bypass mode */
	else
		div = DIV_ROUND_UP(sclk, 2 * freq);

	writel(host, DWMCI_CLKENA, 0);
	writel(host, DWMCI_CLKSRC, 0);

	writel(host, DWMCI_CLKDIV, div);
	writel(host, DWMCI_CMD, DWMCI_CMD_PRV_DAT_WAIT |
			DWMCI_CMD_UPD_CLK | DWMCI_CMD_START);

	do {
		status = readl(host, DWMCI_CMD);
		if (timeout-- < 0) {
			printf("%s: Timeout!\n", __func__);
			return -1;
		}
	} while (status & DWMCI_CMD_START);

	writel(host, DWMCI_CLKENA, DWMCI_CLKEN_ENABLE |
			DWMCI_CLKEN_LOW_PWR);

	writel(host, DWMCI_CMD, DWMCI_CMD_PRV_DAT_WAIT |
			DWMCI_CMD_UPD_CLK | DWMCI_CMD_START);

	timeout = 10000;
	do {
		status = readl(host, DWMCI_CMD);
		if (timeout-- < 0) {
			printf("%s: Timeout!\n", __func__);
			return -1;
		}
	} while (status & DWMCI_CMD_START);

	host->clock = freq;

	return 0;
}

int mmc_init() {

    host = &mmc1;
    host->ioaddr = (void*)JH7110_SDIO1_BASE;
    host->bus_hz = 50000000;
    host->fifo_mode = 1;

    printf("[MMC] Hardware Configuration Register 0x%X\n",readl(host, DWMCI_HCON)); 

    writel(host, DWMCI_PWREN, 1);

    if (!dw_wait_reset(host, DWMCI_RESET_ALL)) {
		printf("%s[%d] Fail-reset!!\n", __func__, __LINE__);
		return -1;
	}

	host->flags = 1 << DW_MMC_CARD_NEED_INIT;

	/* Enumerate at 400KHz */
	dw_setup_bus(host, 4000000);

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