#include <common.h>
#include <jh7110_memmap.h>
#include <sys/riscv.h>
#include <mmc.h>
#include <wait_bit.h>

#include "dwmmc.h"

dw_mmc *host;

dw_mmc dw_mmc0;

static inline void writel(dw_mmc *host, int reg, u32 val){
    putreg32(val, (uint64_t)host->ioaddr+reg);
}

static inline u32 readl(dw_mmc *host, int reg){
    return getreg32((uint64_t)host->ioaddr+reg);
}


static inline int __test_and_clear_bit_1(int nr, void *addr)
{
	int mask, retval;
	unsigned int *a = (unsigned int *)addr;

	a += nr >> 5;
	mask = 1 << (nr & 0x1f);
	retval = (mask & *a) != 0;
	*a &= ~mask;

	return retval;
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

static int dw_fifo_ready(struct dwmmc *host, u32 bit, u32 *len)
{
	u32 timeout = 20000;

	*len = readl(host, DWMCI_STATUS);
	while (--timeout && (*len & bit)) {
		udelay(200);
		*len = readl(host, DWMCI_STATUS);
	}

	if (!timeout) {
		printf("%s: FIFO underflow timeout\n", __func__);
		return -1;
	}

	return 0;
}

static unsigned int dw_get_timeout(struct mmc *mmc, const unsigned int size)
{
	unsigned int timeout;

	timeout = size * 8;	/* counting in bits */
	timeout *= 10;		/* wait 10 times as long */
	timeout /= mmc->clock;
	timeout /= mmc->bus_width;
	timeout /= mmc->ddr_mode ? 2 : 1;
	timeout *= 1000;	/* counting in msec */
	timeout = (timeout < 1000) ? 1000 : timeout;

	return timeout;
}

static int dw_data_transfer(struct dwmmc *host, struct mmc_data *data)
{
	struct mmc *mmc = host->mmc;
	int ret = 0;
	u32 timeout, mask, size, i, len = 0;
	u32 *buf = NULL;
	unsigned long start = timer_get_count();
	u32 fifo_depth = (((host->fifoth_val & RX_WMARK_MASK) >>
			    RX_WMARK_SHIFT) + 1) * 2;

	size = data->blocksize * data->blocks;
	if (data->flags == MMC_DATA_READ)
		buf = (unsigned int *)data->dest;
	else
		buf = (unsigned int *)data->src;

	timeout = dw_get_timeout(mmc, size);

	size /= 4;

	for (;;) {
		mask = readl(host, DWMCI_RINTSTS);
		/* Error during data transfer. */
		if (mask & (DWMCI_DATA_ERR | DWMCI_DATA_TOUT)) {
			printf("%s: DATA ERROR!\n", __func__);
			ret = -1;
			break;
		}

		if (host->fifo_mode && size) {
			len = 0;
			if (data->flags == MMC_DATA_READ &&
			    (mask & (DWMCI_INTMSK_RXDR | DWMCI_INTMSK_DTO))) {
				writel(host, DWMCI_RINTSTS,
					     DWMCI_INTMSK_RXDR | DWMCI_INTMSK_DTO);
				while (size) {
					ret = dw_fifo_ready(host,
							DWMCI_FIFO_EMPTY,
							&len);
					if (ret < 0)
						break;

					len = (len >> DWMCI_FIFO_SHIFT) &
						    DWMCI_FIFO_MASK;
					len = min(size, len);
					for (i = 0; i < len; i++)
						*buf++ =
						readl(host, DWMCI_DATA);
					size = size > len ? (size - len) : 0;
				}
			} else if (data->flags == MMC_DATA_WRITE &&
				   (mask & DWMCI_INTMSK_TXDR)) {
				while (size) {
					ret = dw_fifo_ready(host,
							DWMCI_FIFO_FULL,
							&len);
					if (ret < 0)
						break;

					len = fifo_depth - ((len >>
						   DWMCI_FIFO_SHIFT) &
						   DWMCI_FIFO_MASK);
					len = min(size, len);
					for (i = 0; i < len; i++)
						writel(host, DWMCI_DATA,
							     *buf++);
					size = size > len ? (size - len) : 0;
				}
				writel(host, DWMCI_RINTSTS,
					     DWMCI_INTMSK_TXDR);
			}
		}

		/* Data arrived correctly. */
		if (mask & DWMCI_INTMSK_DTO) {
			ret = 0;
			break;
		}

		/* Check for timeout. */
		if (timer_get_count() - start > timeout) {
			printf("%s: Timeout waiting for data!\n",
			      __func__);
			ret = -1;
			break;
		}
	}

	writel(host, DWMCI_RINTSTS, mask);

	return ret;
}


int dw_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
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

// usig fifo mode

	if (data) {
		if (host->fifo_mode) {
			writel(host, DWMCI_BLKSIZ, data->blocksize);
			writel(host, DWMCI_BYTCNT,
				     data->blocksize * data->blocks);
			dw_wait_reset(host, DWMCI_CTRL_FIFO_RESET);
		} else {
			panic("MMC not fifo");
/*			
			if (data->flags == MMC_DATA_READ) {
				ret = bounce_buffer_start(&bbstate,
						(void*)data->dest,
						data->blocksize *
						data->blocks, GEN_BB_WRITE);
			} else {
				ret = bounce_buffer_start(&bbstate,
						(void*)data->src,
						data->blocksize *
						data->blocks, GEN_BB_READ);
			}

			if (ret)
				return ret;

			dwmci_prepare_data(host, data, cur_idmac,
					   bbstate.bounce_buffer);
*/
		}
	}

	writel(host, DWMCI_CMDARG, cmd->cmdarg);

//	if (data)
//		flags = dw_set_transfer_mode(host, data);

	if ((cmd->resp_type & MMC_RSP_136) && (cmd->resp_type & MMC_RSP_BUSY))
		return -1;

	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION)
		flags |= DWMCI_CMD_ABORT_STOP;
	else
		flags |= DWMCI_CMD_PRV_DAT_WAIT;

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		flags |= DWMCI_CMD_RESP_EXP;
		if (cmd->resp_type & MMC_RSP_136)
			flags |= DWMCI_CMD_RESP_LENGTH;
	}

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= DWMCI_CMD_CHECK_CRC;

	if (__test_and_clear_bit_1(DW_MMC_CARD_NEED_INIT, &host->flags))
		flags |= DWMCI_CMD_SEND_INIT;

	flags |= (cmd->cmdidx | DWMCI_CMD_START | DWMCI_CMD_USE_HOLD_REG);

	printf("Sending CMD%d\n",cmd->cmdidx);

	writel(host, DWMCI_CMD, flags);

	for (i = 0; i < retry; i++) {
		mask = readl(host, DWMCI_RINTSTS);
		if (mask & DWMCI_INTMSK_CDONE) {
			if (!data)
				writel(host, DWMCI_RINTSTS, mask);
			break;
		}
	}

	if (i == retry) {
		printf("%s: Timeout.\n", __func__);
		return -1;
	}

	if (mask & DWMCI_INTMSK_RTO) {
		/*
		 * Timeout here is not necessarily fatal. (e)MMC cards
		 * will splat here when they receive CMD55 as they do
		 * not support this command and that is exactly the way
		 * to tell them apart from SD cards. Thus, this output
		 * below shall be debug(). eMMC cards also do not favor
		 * CMD8, please keep that in mind.
		 */
		printf("%s: Response Timeout.\n", __func__);
		return -1;
	} else if (mask & DWMCI_INTMSK_RE) {
		printf("%s: Response Error.\n", __func__);
		return -1;
	} else if ((cmd->resp_type & MMC_RSP_CRC) &&
		   (mask & DWMCI_INTMSK_RCRC)) {
		printf("%s: Response CRC Error.\n", __func__);
		return -1;
	}


	if (cmd->resp_type & MMC_RSP_PRESENT) {
		if (cmd->resp_type & MMC_RSP_136) {
			cmd->response[0] = readl(host, DWMCI_RESP3);
			cmd->response[1] = readl(host, DWMCI_RESP2);
			cmd->response[2] = readl(host, DWMCI_RESP1);
			cmd->response[3] = readl(host, DWMCI_RESP0);
		} else {
			cmd->response[0] = readl(host, DWMCI_RESP0);
		}
	}

	if (data) {
		ret = dw_data_transfer(host, data);

		/* only dma mode need it */
		if (!host->fifo_mode) {
			if (data->flags == MMC_DATA_READ)
				mask = DWMCI_IDINTEN_RI;
			else
				mask = DWMCI_IDINTEN_TI;
			ret = wait_for_bit_le32(host->ioaddr + DWMCI_IDSTS,
						mask, true, 1000, false);
			if (ret)
				printf("%s: DWMCI_IDINTEN mask 0x%x timeout.\n",
				      __func__, mask);
			/* clear interrupts */
			writel(host, DWMCI_IDSTS, DWMCI_IDINTEN_MASK);

			ctrl = readl(host, DWMCI_CTRL);
			ctrl &= ~(DWMCI_DMA_EN);
			writel(host, DWMCI_CTRL, ctrl);
//			bounce_buffer_stop(&bbstate);
		}
	}

	udelay(100);

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

int dw_set_ios(struct mmc *mmc)
{
	struct dwmmc *host = (struct dwmmc *)mmc->priv;
	u32 ctype, regs;

	printf("Buswidth = %d, clock: %d\n", mmc->bus_width, mmc->clock);

	dw_setup_bus(host, mmc->clock);
	switch (mmc->bus_width) {
	case 8:
		ctype = DWMCI_CTYPE_8BIT;
		break;
	case 4:
		ctype = DWMCI_CTYPE_4BIT;
		break;
	default:
		ctype = DWMCI_CTYPE_1BIT;
		break;
	}

	writel(host, DWMCI_CTYPE, ctype);

	regs = readl(host, DWMCI_UHS_REG);
	if (mmc->ddr_mode)
		regs |= DWMCI_DDR_MODE;
	else
		regs &= ~DWMCI_DDR_MODE;

	writel(host, DWMCI_UHS_REG, regs);


	if (host->clksel) {
		int ret;

		ret = host->clksel(host);
		if (ret)
			return ret;
	}
/*
#if CONFIG_IS_ENABLED(DM_REGULATOR)
	if (mmc->vqmmc_supply) {
		int ret;

		if (mmc->signal_voltage == MMC_SIGNAL_VOLTAGE_180)
			regulator_set_value(mmc->vqmmc_supply, 1800000);
		else
			regulator_set_value(mmc->vqmmc_supply, 3300000);

		ret = regulator_set_enable_if_allowed(mmc->vqmmc_supply, true);
		if (ret)
			return ret;
	}
#endif
*/
	return 0;
}

int dw_mmc_init(struct mmc *mmc) {

    host = &dw_mmc0;
    host->ioaddr = (void*)JH7110_SDIO1_BASE;
    host->bus_hz = 50000000;
    host->fifo_mode = 1;

	host->mmc = mmc;
	mmc->priv = host;

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