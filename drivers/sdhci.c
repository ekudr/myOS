// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2011, Marvell Semiconductor Inc.
 * Lei Wen <leiwen@marvell.com>
 *
 * Back ported to the 8xx platform (from the 8260 platform) by
 * Murray.Jensen@cmst.csiro.au, 27-Jan-01.
 */

#include <common.h>
#include <linux/errno.h>
#include <sdhci.h>
#include <mmc.h>
#include <asm/fls.h>

static void 
sdhci_reset(sdhci_host_t *host, uint8_t mask)
{
	unsigned long timeout;

	/* Wait max 100 ms */
	timeout = 100;
	sdhci_writeb(host, mask, SDHCI_SOFTWARE_RESET);
	while (sdhci_readb(host, SDHCI_SOFTWARE_RESET) & mask) {
		if (timeout == 0) {
			printf("[SDHCI] %s: Reset 0x%x never completed.\n",
			       __func__, (int)mask);
			return;
		}
		timeout--;
		udelay(1000);
	}
}

static void 
sdhci_cmd_done(sdhci_host_t *host, struct mmc_cmd *cmd) {
	int i;
	if (cmd->resp_type & MMC_RSP_136) {
		/* CRC is stripped so we need to do some shifting. */
		for (i = 0; i < 4; i++) {
			cmd->response[i] = sdhci_readl(host,
					SDHCI_RESPONSE + (3-i)*4) << 8;
			if (i != 3)
				cmd->response[i] |= sdhci_readb(host,
						SDHCI_RESPONSE + (3-i)*4-1);
		}
	} else {
		cmd->response[0] = sdhci_readl(host, SDHCI_RESPONSE);
	}
}

static void 
sdhci_transfer_pio(sdhci_host_t *host, struct mmc_data *data)
{
	int i;
	char *offs;
	for (i = 0; i < data->blocksize; i += 4) {
		offs = data->dest + i;
		if (data->flags == MMC_DATA_READ)
			*(uint32_t *)offs = sdhci_readl(host, SDHCI_BUFFER);
		else
			sdhci_writel(host, *(uint32_t *)offs, SDHCI_BUFFER);
	}
}

static int 
sdhci_transfer_data(sdhci_host_t *host, struct mmc_data *data) {
//	dma_addr_t start_addr = host->start_addr;
	unsigned int stat, rdy, mask, timeout, block = 0;
	bool transfer_done = false;

	timeout = 1000000;
	rdy = SDHCI_INT_SPACE_AVAIL | SDHCI_INT_DATA_AVAIL;
	mask = SDHCI_DATA_AVAILABLE | SDHCI_SPACE_AVAILABLE;
	do {
		stat = sdhci_readl(host, SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR) {
			printf("[SDHCI] %s: Error detected in status(0x%X)!\n",
				 __func__, stat);
			return -EIO;
		}
		if (!transfer_done && (stat & rdy)) {
			if (!(sdhci_readl(host, SDHCI_PRESENT_STATE) & mask))
				continue;
			sdhci_writel(host, rdy, SDHCI_INT_STATUS);
			sdhci_transfer_pio(host, data);
			data->dest += data->blocksize;
			if (++block >= data->blocks) {
				/* Keep looping until the SDHCI_INT_DATA_END is
				 * cleared, even if we finished sending all the
				 * blocks.
				 */
				transfer_done = true;
				continue;
			}
		}
		if ((host->flags & USE_DMA) && !transfer_done &&
		    (stat & SDHCI_INT_DMA_END)) {
                panic("[SDHCI] DMA transfer not supported now :)");
/*
			sdhci_writel(host, SDHCI_INT_DMA_END, SDHCI_INT_STATUS);
			if (host->flags & USE_SDMA) {
				start_addr &=
				~(SDHCI_DEFAULT_BOUNDARY_SIZE - 1);
				start_addr += SDHCI_DEFAULT_BOUNDARY_SIZE;
				start_addr = dev_phys_to_bus(mmc_to_dev(host->mmc),
							     start_addr);
				sdhci_writel(host, start_addr, SDHCI_DMA_ADDRESS);
			}
*/            
		}
		if (timeout-- > 0)
			udelay(10);
		else {
			printf("[SDHCI] %s: Transfer data timeout\n", __func__);
			return -ETIMEDOUT;
		}
	} while (!(stat & SDHCI_INT_DATA_END));
/*
#if (defined(CONFIG_MMC_SDHCI_SDMA) || CONFIG_IS_ENABLED(MMC_SDHCI_ADMA))
	dma_unmap_single(host->start_addr, data->blocks * data->blocksize,
			 mmc_get_dma_dir(data));
#endif
*/
	return 0;
}

/*
 * No command will be sent by driver if card is busy, so driver must wait
 * for card ready state.
 * Every time when card is busy after timeout then (last) timeout value will be
 * increased twice but only if it doesn't exceed global defined maximum.
 * Each function call will use last timeout value.
 */
#define SDHCI_CMD_MAX_TIMEOUT			3200
#define SDHCI_CMD_DEFAULT_TIMEOUT		100
#define SDHCI_READ_STATUS_TIMEOUT		1000

static int sdhci_send_command(mmc_t *mmc, struct mmc_cmd *cmd,
			      struct mmc_data *data) {

	sdhci_host_t *host = (sdhci_host_t *)mmc->priv;
	unsigned int stat = 0;
	int ret = 0;
	int trans_bytes = 0, is_aligned = 1;
	uint32_t mask, flags, mode;
	unsigned int time = 0;
//	int mmc_dev = mmc_get_blk_desc(mmc)->devnum;
	uint64_t start = get_timer(0);

//	host->start_addr = 0;
	/* Timeout unit - ms */
	static unsigned int cmd_timeout = SDHCI_CMD_DEFAULT_TIMEOUT;

	mask = SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT;

	/* We shouldn't wait for data inihibit for stop commands, even
	   though they might use busy signaling */
	if (cmd->cmdidx == MMC_CMD_STOP_TRANSMISSION ||
	    ((cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK ||
	      cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200) && !data))
		mask &= ~SDHCI_DATA_INHIBIT;

	while (sdhci_readl(host, SDHCI_PRESENT_STATE) & mask) {
		if (time >= cmd_timeout) {
			printf("[SDHCI] %s: MMC busy ", __func__);
			if (2 * cmd_timeout <= SDHCI_CMD_MAX_TIMEOUT) {
				cmd_timeout += cmd_timeout;
				printf("timeout increasing to: %u ms.\n",
				       cmd_timeout);
			} else {
				printf("timeout.\n");
				return -ECOMM;
			}
		}
		time++;
		udelay(1000);
	}

	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);

	mask = SDHCI_INT_RESPONSE;
	if ((cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK ||
	     cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200) && !data)
		mask = SDHCI_INT_DATA_AVAIL;

	if (!(cmd->resp_type & MMC_RSP_PRESENT))
		flags = SDHCI_CMD_RESP_NONE;
	else if (cmd->resp_type & MMC_RSP_136)
		flags = SDHCI_CMD_RESP_LONG;
	else if (cmd->resp_type & MMC_RSP_BUSY) {
		flags = SDHCI_CMD_RESP_SHORT_BUSY;
		mask |= SDHCI_INT_DATA_END;
	} else
		flags = SDHCI_CMD_RESP_SHORT;

	if (cmd->resp_type & MMC_RSP_CRC)
		flags |= SDHCI_CMD_CRC;
	if (cmd->resp_type & MMC_RSP_OPCODE)
		flags |= SDHCI_CMD_INDEX;
	if (data || cmd->cmdidx ==  MMC_CMD_SEND_TUNING_BLOCK ||
	    cmd->cmdidx == MMC_CMD_SEND_TUNING_BLOCK_HS200)
		flags |= SDHCI_CMD_DATA;

	/* Set Transfer mode regarding to data flag */
	if (data) {
		sdhci_writeb(host, 0xe, SDHCI_TIMEOUT_CONTROL);
		mode = SDHCI_TRNS_BLK_CNT_EN;
		trans_bytes = data->blocks * data->blocksize;
		if (data->blocks > 1)
			mode |= SDHCI_TRNS_MULTI;

		if (data->flags == MMC_DATA_READ)
			mode |= SDHCI_TRNS_READ;

		if (host->flags & USE_DMA) {
            panic("[SDHCI] DMA not working");
//			mode |= SDHCI_TRNS_DMA;
//			sdhci_prepare_dma(host, data, &is_aligned, trans_bytes);
		}

		sdhci_writew(host, SDHCI_MAKE_BLKSZ(SDHCI_DEFAULT_BOUNDARY_ARG,
				data->blocksize),
				SDHCI_BLOCK_SIZE);
		sdhci_writew(host, data->blocks, SDHCI_BLOCK_COUNT);
		sdhci_writew(host, mode, SDHCI_TRANSFER_MODE);
	} else if (cmd->resp_type & MMC_RSP_BUSY) {
		sdhci_writeb(host, 0xe, SDHCI_TIMEOUT_CONTROL);
	}

	sdhci_writel(host, cmd->cmdarg, SDHCI_ARGUMENT);
	sdhci_writew(host, SDHCI_MAKE_CMD(cmd->cmdidx, flags), SDHCI_COMMAND);
	start = get_timer(0);
	do {
		stat = sdhci_readl(host, SDHCI_INT_STATUS);
		if (stat & SDHCI_INT_ERROR)
			break;

		if (get_timer(start) >= SDHCI_READ_STATUS_TIMEOUT) {
			if (host->quirks & SDHCI_QUIRK_BROKEN_R1B) {
				return 0;
			} else {
				printf("[SDHCI] %s: Timeout for status update!\n",
				       __func__);
				return -ETIMEDOUT;
			}
		}
	} while ((stat & mask) != mask);

	if ((stat & (SDHCI_INT_ERROR | mask)) == mask) {
		sdhci_cmd_done(host, cmd);
		sdhci_writel(host, mask, SDHCI_INT_STATUS);
	} else
		ret = -1;

	if (!ret && data)
		ret = sdhci_transfer_data(host, data);

	if (host->quirks & SDHCI_QUIRK_WAIT_SEND_CMD)
		udelay(1000);

	stat = sdhci_readl(host, SDHCI_INT_STATUS);
	sdhci_writel(host, SDHCI_INT_ALL_MASK, SDHCI_INT_STATUS);
	if (!ret) {
		if ((host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) &&
				!is_aligned && (data->flags == MMC_DATA_READ))
			memcpy(data->dest, host->align_buffer, trans_bytes);
		return 0;
	}

	sdhci_reset(host, SDHCI_RESET_CMD);
	sdhci_reset(host, SDHCI_RESET_DATA);
	if (stat & SDHCI_INT_TIMEOUT)
		return -ETIMEDOUT;
	else
		return -ECOMM;
}

int 
sdhci_set_clock(mmc_t *mmc, unsigned int clock) {
	sdhci_host_t *host = mmc->priv;
	unsigned int div, clk = 0, timeout;
	int ret;

	/* Wait max 20 ms */
	timeout = 200;
	while (sdhci_readl(host, SDHCI_PRESENT_STATE) &
			   (SDHCI_CMD_INHIBIT | SDHCI_DATA_INHIBIT)) {
		if (timeout == 0) {
			printf("[SDHCI] %s: Timeout to wait cmd & data inhibit\n",
			       __func__);
			return -EBUSY;
		}

		timeout--;
		udelay(100);
	}

	sdhci_writew(host, 0, SDHCI_CLOCK_CONTROL);

	if (clock == 0)
		return 0;

	if (host->ops && host->ops->set_delay) {
		ret = host->ops->set_delay(host);
		if (ret) {
			printf("[SDHCI] %s: Error while setting tap delay\n", __func__);
			return ret;
		}
	}

	if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) {
		/*
		 * Check if the Host Controller supports Programmable Clock
		 * Mode.
		 */
		if (host->clk_mul) {
			for (div = 1; div <= 1024; div++) {
				if ((host->max_clk / div) <= clock)
					break;
			}

			/*
			 * Set Programmable Clock Mode in the Clock
			 * Control register.
			 */
			clk = SDHCI_PROG_CLOCK_MODE;
			div--;
		} else {
			/* Version 3.00 divisors must be a multiple of 2. */
			if (host->max_clk <= clock) {
				div = 1;
			} else {
				for (div = 2;
				     div < SDHCI_MAX_DIV_SPEC_300;
				     div += 2) {
					if ((host->max_clk / div) <= clock)
						break;
				}
			}
			div >>= 1;
		}
	} else {
		/* Version 2.00 divisors must be a power of 2. */
		for (div = 1; div < SDHCI_MAX_DIV_SPEC_200; div *= 2) {
			if ((host->max_clk / div) <= clock)
				break;
		}
		div >>= 1;
	}

//	if (host->ops && host->ops->set_clock)
//		host->ops->set_clock(host, div);

	clk |= (div & SDHCI_DIV_MASK) << SDHCI_DIVIDER_SHIFT;
	clk |= ((div & SDHCI_DIV_HI_MASK) >> SDHCI_DIV_MASK_LEN)
		<< SDHCI_DIVIDER_HI_SHIFT;
	clk |= SDHCI_CLOCK_INT_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);

	/* Wait max 20 ms */
	timeout = 20;
	while (!((clk = sdhci_readw(host, SDHCI_CLOCK_CONTROL))
		& SDHCI_CLOCK_INT_STABLE)) {
		if (timeout == 0) {
			printf("[SDHCI] %s: Internal clock never stabilised.\n",
			       __func__);
			return -EBUSY;
		}
		timeout--;
		udelay(1000);
	}

	clk |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, clk, SDHCI_CLOCK_CONTROL);
	return 0;
}

static void 
sdhci_set_power(sdhci_host_t *host, unsigned short power) {
	uint8_t pwr = 0;

	if (power != (unsigned short)-1) {
		switch (1 << power) {
		case MMC_VDD_165_195:
			pwr = SDHCI_POWER_180;
			break;
		case MMC_VDD_29_30:
		case MMC_VDD_30_31:
			pwr = SDHCI_POWER_300;
			break;
		case MMC_VDD_32_33:
		case MMC_VDD_33_34:
			pwr = SDHCI_POWER_330;
			break;
		}
	}

	if (pwr == 0) {
		sdhci_writeb(host, 0, SDHCI_POWER_CONTROL);
		return;
	}

	pwr |= SDHCI_POWER_ON;

	sdhci_writeb(host, pwr, SDHCI_POWER_CONTROL);
}

void 
sdhci_set_uhs_timing(sdhci_host_t *host) {
	mmc_t *mmc = host->mmc;
	uint32_t reg;

	reg = sdhci_readw(host, SDHCI_HOST_CONTROL2);
	reg &= ~SDHCI_CTRL_UHS_MASK;

	switch (mmc->selected_mode) {
	case UHS_SDR50:
	case MMC_HS_52:
		reg |= SDHCI_CTRL_UHS_SDR50;
		break;
	case UHS_DDR50:
	case MMC_DDR_52:
		reg |= SDHCI_CTRL_UHS_DDR50;
		break;
	case UHS_SDR104:
	case MMC_HS_200:
		reg |= SDHCI_CTRL_UHS_SDR104;
		break;
	case MMC_HS_400:
		reg |= SDHCI_CTRL_HS400;
		break;
	default:
		reg |= SDHCI_CTRL_UHS_SDR12;
	}

	sdhci_writew(host, reg, SDHCI_HOST_CONTROL2);
}

static void 
sdhci_set_voltage(sdhci_host_t *host) {
	if (0/*IS_ENABLED(CONFIG_MMC_IO_VOLTAGE)*/) {
		mmc_t *mmc = (mmc_t *)host->mmc;
		uint32_t ctrl;

		ctrl = sdhci_readw(host, SDHCI_HOST_CONTROL2);

		switch (mmc->signal_voltage) {
		case MMC_SIGNAL_VOLTAGE_330:
/*        
#if CONFIG_IS_ENABLED(DM_REGULATOR)
			if (mmc->vqmmc_supply) {
				if (regulator_set_enable_if_allowed(mmc->vqmmc_supply, false)) {
					pr_err("failed to disable vqmmc-supply\n");
					return;
				}

				if (regulator_set_value(mmc->vqmmc_supply, 3300000)) {
					pr_err("failed to set vqmmc-voltage to 3.3V\n");
					return;
				}

				if (regulator_set_enable_if_allowed(mmc->vqmmc_supply, true)) {
					pr_err("failed to enable vqmmc-supply\n");
					return;
				}
			}
#endif
*/
			if (IS_SD(mmc)) {
				ctrl &= ~SDHCI_CTRL_VDD_180;
				sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
			}

			/* Wait for 5ms */
			mdelay(5);

			/* 3.3V regulator output should be stable within 5 ms */
			if (IS_SD(mmc)) {
				if (ctrl & SDHCI_CTRL_VDD_180) {
					printf("[SDHCI] 3.3V regulator output did not become stable\n");
					return;
				}
			}

			break;
		case MMC_SIGNAL_VOLTAGE_180:
/*        
#if CONFIG_IS_ENABLED(DM_REGULATOR)
			if (mmc->vqmmc_supply) {
				if (regulator_set_enable_if_allowed(mmc->vqmmc_supply, false)) {
					pr_err("failed to disable vqmmc-supply\n");
					return;
				}

				if (regulator_set_value(mmc->vqmmc_supply, 1800000)) {
					pr_err("failed to set vqmmc-voltage to 1.8V\n");
					return;
				}

				if (regulator_set_enable_if_allowed(mmc->vqmmc_supply, true)) {
					pr_err("failed to enable vqmmc-supply\n");
					return;
				}
			}
#endif
*/
			if (IS_SD(mmc)) {
				ctrl |= SDHCI_CTRL_VDD_180;
				sdhci_writew(host, ctrl, SDHCI_HOST_CONTROL2);
			}

			/* Wait for 5 ms */
			mdelay(5);

			/* 1.8V regulator output has to be stable within 5 ms */
			if (IS_SD(mmc)) {
				if (!(ctrl & SDHCI_CTRL_VDD_180)) {
					printf("[SDHCI] 1.8V regulator output did not become stable\n");
					return;
				}
			}

			break;
		default:
			/* No signal voltage switch required */
			return;
		}
	}
}

void 
sdhci_set_control_reg(sdhci_host_t *host) {
	sdhci_set_voltage(host);
	sdhci_set_uhs_timing(host);
}

static int 
sdhci_set_ios(mmc_t *mmc) {
	uint32_t ctrl;
	sdhci_host_t *host = mmc->priv;
	bool no_hispd_bit = false;

	if (host->ops && host->ops->set_control_reg)
		host->ops->set_control_reg(host);

	if (mmc->clock != host->clock)
		sdhci_set_clock(mmc, mmc->clock);

	if (mmc->clk_disable)
		sdhci_set_clock(mmc, 0);

	/* Set bus width */
	ctrl = sdhci_readb(host, SDHCI_HOST_CONTROL);
	if (mmc->bus_width == 8) {
		ctrl &= ~SDHCI_CTRL_4BITBUS;
		if ((SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) ||
				(host->quirks & SDHCI_QUIRK_USE_WIDE8))
			ctrl |= SDHCI_CTRL_8BITBUS;
	} else {
		if ((SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) ||
				(host->quirks & SDHCI_QUIRK_USE_WIDE8))
			ctrl &= ~SDHCI_CTRL_8BITBUS;
		if (mmc->bus_width == 4)
			ctrl |= SDHCI_CTRL_4BITBUS;
		else
			ctrl &= ~SDHCI_CTRL_4BITBUS;
	}

	if ((host->quirks & SDHCI_QUIRK_NO_HISPD_BIT) ||
	    (host->quirks & SDHCI_QUIRK_BROKEN_HISPD_MODE)) {
		ctrl &= ~SDHCI_CTRL_HISPD;
		no_hispd_bit = true;
	}

	if (!no_hispd_bit) {
		if (mmc->selected_mode == MMC_HS ||
		    mmc->selected_mode == SD_HS ||
		    mmc->selected_mode == MMC_DDR_52 ||
		    mmc->selected_mode == MMC_HS_200 ||
		    mmc->selected_mode == MMC_HS_400 ||
		    mmc->selected_mode == UHS_SDR25 ||
		    mmc->selected_mode == UHS_SDR50 ||
		    mmc->selected_mode == UHS_SDR104 ||
		    mmc->selected_mode == UHS_DDR50)
			ctrl |= SDHCI_CTRL_HISPD;
		else
			ctrl &= ~SDHCI_CTRL_HISPD;
	}

	sdhci_writeb(host, ctrl, SDHCI_HOST_CONTROL);

	/* If available, call the driver specific "post" set_ios() function */
	if (host->ops && host->ops->set_ios_post)
		return host->ops->set_ios_post(host);

	return 0;
}


int sdhci_init(struct mmc *mmc)
{
	struct sdhci_host *host = mmc->priv;


	sdhci_reset(host, SDHCI_RESET_ALL);


	if (host->quirks & SDHCI_QUIRK_32BIT_DMA_ADDR) {
		host->align_buffer = 0/*memalign(8, 512 * 1024)*/;
		if (!host->align_buffer) {
			printf("[SDHCI] %s: Aligned buffer alloc failed!!!\n",
			       __func__);
			return -ENOMEM;
		}
	}


	sdhci_set_power(host, fls(mmc->cfg->voltages) - 1);
/*
	if (host->ops && host->ops->get_cd)
		host->ops->get_cd(host);
*/
	/* Enable only interrupts served by the SD controller */
	sdhci_writel(host, SDHCI_INT_DATA_MASK | SDHCI_INT_CMD_MASK,
		     SDHCI_INT_ENABLE);
	/* Mask all sdhci interrupt sources */
	sdhci_writel(host, 0x0, SDHCI_SIGNAL_ENABLE);

	return 0;
}

mmc_ops_t sdhci_ops = {
	.send_cmd	= sdhci_send_command,
	.set_ios	= sdhci_set_ios,
	.init		= sdhci_init,
};

int 
sdhci_setup_cfg(mmc_config_t *cfg, sdhci_host_t *host,
		uint32_t f_max, uint32_t f_min) {
	uint32_t caps, caps_1 = 0;

	caps = sdhci_readl(host, SDHCI_CAPABILITIES);

	printf("[SDHCI] %s, caps: 0x%x\n", __func__, caps);
/*
#ifdef CONFIG_MMC_SDHCI_SDMA
	if ((caps & SDHCI_CAN_DO_SDMA)) {
		host->flags |= USE_SDMA;
	} else {
		debug("%s: Your controller doesn't support SDMA!!\n",
		      __func__);
	}
#endif
#if CONFIG_IS_ENABLED(MMC_SDHCI_ADMA)
	if (!(caps & SDHCI_CAN_DO_ADMA2)) {
		pr_err("%s: Your controller doesn't support SDMA!!\n",
		       __func__);
		return -EINVAL;
	}
	host->adma_desc_table = sdhci_adma_init();
	host->adma_addr = (dma_addr_t)host->adma_desc_table;

#ifdef CONFIG_DMA_ADDR_T_64BIT
	host->flags |= USE_ADMA64;
#else
	host->flags |= USE_ADMA;
#endif
#endif
*/
	if (host->quirks & SDHCI_QUIRK_REG32_RW)
		host->version =
			sdhci_readl(host, SDHCI_HOST_VERSION - 2) >> 16;
	else
		host->version = sdhci_readw(host, SDHCI_HOST_VERSION);

	cfg->name = host->name;
    

	cfg->ops = &sdhci_ops;


	/* Check whether the clock multiplier is supported or not */
	if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) {
/*        
#if CONFIG_IS_ENABLED(DM_MMC)
		caps_1 = ~upper_32_bits(dt_caps_mask) &
			 sdhci_readl(host, SDHCI_CAPABILITIES_1);
		caps_1 |= upper_32_bits(dt_caps);
#else
*/
		caps_1 = sdhci_readl(host, SDHCI_CAPABILITIES_1);
//#endif
		printf("[SDHCI] %s, caps_1: 0x%x\n", __func__, caps_1);
		host->clk_mul = (caps_1 & SDHCI_CLOCK_MUL_MASK) >>
				SDHCI_CLOCK_MUL_SHIFT;
	}

	if (host->max_clk == 0) {
		if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300)
			host->max_clk = (caps & SDHCI_CLOCK_V3_BASE_MASK) >>
				SDHCI_CLOCK_BASE_SHIFT;
		else
			host->max_clk = (caps & SDHCI_CLOCK_BASE_MASK) >>
				SDHCI_CLOCK_BASE_SHIFT;
		host->max_clk *= 1000000;
		if (host->clk_mul)
			host->max_clk *= host->clk_mul;
	}
	if (host->max_clk == 0) {
		printf("[SDHCI] %s: Hardware doesn't specify base clock frequency\n",
		       __func__);
		return -EINVAL;
	}
	if (f_max && (f_max < host->max_clk))
		cfg->f_max = f_max;
	else
		cfg->f_max = host->max_clk;
	if (f_min)
		cfg->f_min = f_min;
	else {
		if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300)
			cfg->f_min = cfg->f_max / SDHCI_MAX_DIV_SPEC_300;
		else
			cfg->f_min = cfg->f_max / SDHCI_MAX_DIV_SPEC_200;
	}
	cfg->voltages = 0;
	if (caps & SDHCI_CAN_VDD_330)
		cfg->voltages |= MMC_VDD_32_33 | MMC_VDD_33_34;
	if (caps & SDHCI_CAN_VDD_300)
		cfg->voltages |= MMC_VDD_29_30 | MMC_VDD_30_31;
	if (caps & SDHCI_CAN_VDD_180)
		cfg->voltages |= MMC_VDD_165_195;

	if (host->quirks & SDHCI_QUIRK_BROKEN_VOLTAGE)
		cfg->voltages |= host->voltages;

	if (caps & SDHCI_CAN_DO_HISPD)
		cfg->host_caps |= MMC_MODE_HS | MMC_MODE_HS_52MHz;

	cfg->host_caps |= MMC_MODE_4BIT;

	/* Since Host Controller Version3.0 */
	if (SDHCI_GET_VERSION(host) >= SDHCI_SPEC_300) {
		if (!(caps & SDHCI_CAN_DO_8BIT))
			cfg->host_caps &= ~MMC_MODE_8BIT;
	}

	if (host->quirks & SDHCI_QUIRK_BROKEN_HISPD_MODE) {
		cfg->host_caps &= ~MMC_MODE_HS;
		cfg->host_caps &= ~MMC_MODE_HS_52MHz;
	}

	if (!(cfg->voltages & MMC_VDD_165_195) ||
	    (host->quirks & SDHCI_QUIRK_NO_1_8_V))
		caps_1 &= ~(SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_SDR50 |
			    SDHCI_SUPPORT_DDR50);

	if (caps_1 & (SDHCI_SUPPORT_SDR104 | SDHCI_SUPPORT_SDR50 |
		      SDHCI_SUPPORT_DDR50))
		cfg->host_caps |= MMC_CAP(UHS_SDR12) | MMC_CAP(UHS_SDR25);

	if (caps_1 & SDHCI_SUPPORT_SDR104) {
		cfg->host_caps |= MMC_CAP(UHS_SDR104) | MMC_CAP(UHS_SDR50);
		/*
		 * SD3.0: SDR104 is supported so (for eMMC) the caps2
		 * field can be promoted to support HS200.
		 */
		cfg->host_caps |= MMC_CAP(MMC_HS_200);
	} else if (caps_1 & SDHCI_SUPPORT_SDR50) {
		cfg->host_caps |= MMC_CAP(UHS_SDR50);
	}

	if (caps_1 & SDHCI_SUPPORT_DDR50)
		cfg->host_caps |= MMC_CAP(UHS_DDR50);

	if (host->host_caps)
		cfg->host_caps |= host->host_caps;

	cfg->b_max = 1024;

    debug("[SDHCI] cfg f_min %d f_max %d voltages 0x%lX\n", cfg->f_min, cfg->f_max, cfg->voltages);

	return 0;
}
