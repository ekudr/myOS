// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2008, Freescale Semiconductor, Inc
 * Copyright 2020 NXP
 * Andy Fleming
 *
 * Based vaguely on the Linux code
 */
// based on u-boot

#include <common.h>
#include <mmc.h>


int dw_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data);
int dw_set_ios(struct mmc *mmc);
int dw_mmc_init(struct mmc *mmc);



// supposted we have 1 MMC dev for now
struct mmc mmc0;

void mmmc_trace_before_send(struct mmc *mmc, struct mmc_cmd *cmd)
{
	printf("CMD_SEND:%d\n", cmd->cmdidx);
	printf("\t\tARG\t\t\t 0x%08x\n", cmd->cmdarg);
}

void mmmc_trace_after_send(struct mmc *mmc, struct mmc_cmd *cmd, int ret)
{
	int i;
	u8 *ptr;

	if (ret) {
		printf("\t\tRET\t\t\t %d\n", ret);
	} else {
		switch (cmd->resp_type) {
		case MMC_RSP_NONE:
			printf("\t\tMMC_RSP_NONE\n");
			break;
		case MMC_RSP_R1:
			printf("\t\tMMC_RSP_R1,5,6,7 \t 0x%08x \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R1b:
			printf("\t\tMMC_RSP_R1b\t\t 0x%08x \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R2:
			printf("\t\tMMC_RSP_R2\t\t 0x%08x \n",
				cmd->response[0]);
			printf("\t\t          \t\t 0x%08x \n",
				cmd->response[1]);
			printf("\t\t          \t\t 0x%08x \n",
				cmd->response[2]);
			printf("\t\t          \t\t 0x%08x \n",
				cmd->response[3]);
			printf("\n");
			printf("\t\t\t\t\tDUMPING DATA\n");
			for (i = 0; i < 4; i++) {
				int j;
				printf("\t\t\t\t\t%03d - ", i*4);
				ptr = (u8 *)&cmd->response[i];
				ptr += 3;
				for (j = 0; j < 4; j++)
					printf("%02x ", *ptr--);
				printf("\n");
			}
			break;
		case MMC_RSP_R3:
			printf("\t\tMMC_RSP_R3,4\t\t 0x%08x \n",
				cmd->response[0]);
			break;
		default:
			printf("\t\tERROR MMC rsp not supported\n");
			break;
		}
	}
}

void mmc_trace_state(struct mmc *mmc, struct mmc_cmd *cmd)
{
	int status;

	status = (cmd->response[0] & MMC_STATUS_CURR_STATE) >> 9;
	printf("[MMC] CURR STATE:%d\n", status);
}

const char *mmc_mode_name(enum bus_mode mode)
{
	static const char *const names[] = {
	      [MMC_LEGACY]	= "MMC legacy",
	      [MMC_HS]		= "MMC High Speed (26MHz)",
	      [SD_HS]		= "SD High Speed (50MHz)",
	      [UHS_SDR12]	= "UHS SDR12 (25MHz)",
	      [UHS_SDR25]	= "UHS SDR25 (50MHz)",
	      [UHS_SDR50]	= "UHS SDR50 (100MHz)",
	      [UHS_SDR104]	= "UHS SDR104 (208MHz)",
	      [UHS_DDR50]	= "UHS DDR50 (50MHz)",
	      [MMC_HS_52]	= "MMC High Speed (52MHz)",
	      [MMC_DDR_52]	= "MMC DDR52 (52MHz)",
	      [MMC_HS_200]	= "HS200 (200MHz)",
	      [MMC_HS_400]	= "HS400 (200MHz)",
	      [MMC_HS_400_ES]	= "HS400ES (200MHz)",
	};

	if (mode >= MMC_MODES_END)
		return "Unknown mode";
	else
		return names[mode];
}

static uint32 mmc_mode2freq(struct mmc *mmc, enum bus_mode mode)
{
	static const int freqs[] = {
	      [MMC_LEGACY]	= 25000000,
	      [MMC_HS]		= 26000000,
	      [SD_HS]		= 50000000,
	      [MMC_HS_52]	= 52000000,
	      [MMC_DDR_52]	= 52000000,
	      [UHS_SDR12]	= 25000000,
	      [UHS_SDR25]	= 50000000,
	      [UHS_SDR50]	= 100000000,
	      [UHS_DDR50]	= 50000000,
	      [UHS_SDR104]	= 208000000,
	      [MMC_HS_200]	= 200000000,
	      [MMC_HS_400]	= 200000000,
	      [MMC_HS_400_ES]	= 200000000,
	};

	if (mode == MMC_LEGACY)
		return mmc->legacy_speed;
	else if (mode >= MMC_MODES_END)
		return 0;
	else
		return freqs[mode];
}

int mmc_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data)
{
	int ret;

	mmmc_trace_before_send(mmc, cmd);
//	ret = mmc->cfg->ops->send_cmd(mmc, cmd, data);
    ret = dw_send_cmd(mmc, cmd, data);
	mmmc_trace_after_send(mmc, cmd, ret);

	return ret;
}



static int mmc_select_mode(struct mmc *mmc, enum bus_mode mode)
{
	mmc->selected_mode = mode;
	mmc->tran_speed = mmc_mode2freq(mmc, mode);
	mmc->ddr_mode = mmc_is_mode_ddr(mode);
	printf("[MMC] selecting mode %s (freq : %d MHz)\n", mmc_mode_name(mode),
		 mmc->tran_speed / 1000000);
	return 0;
}

int mmc_set_clock(struct mmc *mmc, uint32 clock, bool disable)
{
	if (!disable) {
		if (clock > mmc->cfg->f_max)
			clock = mmc->cfg->f_max;

		if (clock < mmc->cfg->f_min)
			clock = mmc->cfg->f_min;
	}

	mmc->clock = clock;
	mmc->clk_disable = disable;

	printf("[MMC] clock is %s (%dHz)\n", disable ? "disabled" : "enabled", clock);

	return dw_set_ios(mmc);
}

static int mmc_set_bus_width(struct mmc *mmc, uint32 width)
{
	mmc->bus_width = width;

	return dw_set_ios(mmc);
}

static int mmc_go_idle(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int err;

	udelay(1000);

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	udelay(2000);

	return 0;
}

static int mmc_startup(struct mmc *mmc)
{
	int err, i;
	uint32 mult, freq;
	u64 cmult, csize;
	struct mmc_cmd cmd;
//	struct blk_desc *bdesc;

#ifdef CONFIG_MMC_SPI_CRC_ON
	if (mmc_host_is_spi(mmc)) { /* enable CRC check for spi */
		cmd.cmdidx = MMC_CMD_SPI_CRC_ON_OFF;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 1;
		err = mmc_send_cmd(mmc, &cmd, NULL);
		if (err)
			return err;
	}
#endif

	/* Put the Card in Identify Mode */

//	cmd.cmdidx = mmc_host_is_spi(mmc) ? MMC_CMD_SEND_CID :
//		MMC_CMD_ALL_SEND_CID; /* cmd not supported in spi */
    cmd.cmdidx = MMC_CMD_ALL_SEND_CID;

	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;
/*
	err = mmc_send_cmd_quirks(mmc, &cmd, NULL, MMC_QUIRK_RETRY_SEND_CID, 4);
	if (err)
		return err;

	sbi_memcpy(mmc->cid, cmd.response, 16);
*/
	/*
	 * For MMC cards, set the Relative Address.
	 * For SD cards, get the Relatvie Address.
	 * This also puts the cards into Standby State
	 */

	if (/*!mmc_host_is_spi(mmc)*/1) { /* cmd not supported in spi */
		cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
		cmd.cmdarg = mmc->rca << 16;
		cmd.resp_type = MMC_RSP_R6;

		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;

		if (IS_SD(mmc))
			mmc->rca = (cmd.response[0] >> 16) & 0xffff;
	}

	/* Get the Card-Specific Data */
	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = mmc->rca << 16;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	mmc->csd[0] = cmd.response[0];
	mmc->csd[1] = cmd.response[1];
	mmc->csd[2] = cmd.response[2];
	mmc->csd[3] = cmd.response[3];

	if (mmc->version == MMC_VERSION_UNKNOWN) {
		int version = (cmd.response[0] >> 26) & 0xf;

		switch (version) {
		case 0:
			mmc->version = MMC_VERSION_1_2;
			break;
		case 1:
			mmc->version = MMC_VERSION_1_4;
			break;
		case 2:
			mmc->version = MMC_VERSION_2_2;
			break;
		case 3:
			mmc->version = MMC_VERSION_3;
			break;
		case 4:
			mmc->version = MMC_VERSION_4;
			break;
		default:
			mmc->version = MMC_VERSION_1_2;
			break;
		}
	}

	/* divide frequency by 10, since the mults are 10x bigger */
//	freq = fbase[(cmd.response[0] & 0x7)];
//	mult = multipliers[((cmd.response[0] >> 3) & 0xf)];

//	mmc->legacy_speed = freq * mult;
	mmc_select_mode(mmc, MMC_LEGACY);

	mmc->dsr_imp = ((cmd.response[1] >> 12) & 0x1);
	mmc->read_bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);
/*    
#if CONFIG_IS_ENABLED(MMC_WRITE)

	if (IS_SD(mmc))
		mmc->write_bl_len = mmc->read_bl_len;
	else
		mmc->write_bl_len = 1 << ((cmd.response[3] >> 22) & 0xf);
#endif
*/
	if (mmc->high_capacity) {
		csize = (mmc->csd[1] & 0x3f) << 16
			| (mmc->csd[2] & 0xffff0000) >> 16;
		cmult = 8;
	} else {
		csize = (mmc->csd[1] & 0x3ff) << 2
			| (mmc->csd[2] & 0xc0000000) >> 30;
		cmult = (mmc->csd[2] & 0x00038000) >> 15;
	}

	mmc->capacity_user = (csize + 1) << (cmult + 2);
	mmc->capacity_user *= mmc->read_bl_len;
	mmc->capacity_boot = 0;
	mmc->capacity_rpmb = 0;
	for (i = 0; i < 4; i++)
		mmc->capacity_gp[i] = 0;

	if (mmc->read_bl_len > MMC_MAX_BLOCK_LEN)
		mmc->read_bl_len = MMC_MAX_BLOCK_LEN;
/*
#if CONFIG_IS_ENABLED(MMC_WRITE)
	if (mmc->write_bl_len > MMC_MAX_BLOCK_LEN)
		mmc->write_bl_len = MMC_MAX_BLOCK_LEN;
#endif
*/
	if ((mmc->dsr_imp) && (0xffffffff != mmc->dsr)) {
		cmd.cmdidx = MMC_CMD_SET_DSR;
		cmd.cmdarg = (mmc->dsr & 0xffff) << 16;
		cmd.resp_type = MMC_RSP_NONE;
		if (mmc_send_cmd(mmc, &cmd, NULL))
			printf("[MMC] SET_DSR failed\n");
	}

	/* Select the card, and put it into Transfer Mode */
	if (1/*!mmc_host_is_spi(mmc)*/) { /* cmd not supported in spi */
		cmd.cmdidx = MMC_CMD_SELECT_CARD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = mmc->rca << 16;
		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;
	}

	/*
	 * For SD, its erase group is always one sector
	 */
/*   
#if CONFIG_IS_ENABLED(MMC_WRITE)
	mmc->erase_grp_size = 1;
#endif
*/ 
	mmc->part_config = MMCPART_NOAVAILABLE;
/*
	err = mmc_startup_v4(mmc);
	if (err)
		return err;
 */       
/*
	err = mmc_set_capacity(mmc, mmc_get_blk_desc(mmc)->hwpart);
	if (err)
		return err;
*/
#if 1 //CONFIG_IS_ENABLED(MMC_TINY)
	mmc_set_clock(mmc, mmc->legacy_speed, false);
	mmc_select_mode(mmc, MMC_LEGACY);
	mmc_set_bus_width(mmc, 1);
#else
	if (IS_SD(mmc)) {
		err = sd_get_capabilities(mmc);
		if (err)
			return err;
		err = sd_select_mode_and_width(mmc, mmc->card_caps);
	} else {
		err = mmc_get_capabilities(mmc);
		if (err)
			return err;
		err = mmc_select_mode_and_width(mmc, mmc->card_caps);
	}
#endif
	if (err)
		return err;

	mmc->best_mode = mmc->selected_mode;

	/* Fix the block length for DDR mode */
	if (mmc->ddr_mode) {
		mmc->read_bl_len = MMC_MAX_BLOCK_LEN;
/*        
#if CONFIG_IS_ENABLED(MMC_WRITE)
		mmc->write_bl_len = MMC_MAX_BLOCK_LEN;
#endif
*/
	}

	/* fill in device description */
/*    
	bdesc = mmc_get_blk_desc(mmc);
	bdesc->lun = 0;
	bdesc->hwpart = 0;
	bdesc->type = 0;
	bdesc->blksz = mmc->read_bl_len;
	bdesc->log2blksz = LOG2(bdesc->blksz);
	bdesc->lba = lldiv(mmc->capacity, mmc->read_bl_len);
*/
/*   
#if !defined(CONFIG_SPL_BUILD) || \
		(defined(CONFIG_SPL_LIBCOMMON_SUPPORT) && \
		!CONFIG_IS_ENABLED(USE_TINY_PRINTF))
	sprintf(bdesc->vendor, "Man %06x Snr %04x%04x",
		mmc->cid[0] >> 24, (mmc->cid[2] & 0xffff),
		(mmc->cid[3] >> 16) & 0xffff);
	sprintf(bdesc->product, "%c%c%c%c%c%c", mmc->cid[0] & 0xff,
		(mmc->cid[1] >> 24), (mmc->cid[1] >> 16) & 0xff,
		(mmc->cid[1] >> 8) & 0xff, mmc->cid[1] & 0xff,
		(mmc->cid[2] >> 24) & 0xff);
	sprintf(bdesc->revision, "%d.%d", (mmc->cid[2] >> 20) & 0xf,
		(mmc->cid[2] >> 16) & 0xf);
#else
	bdesc->vendor[0] = 0;
	bdesc->product[0] = 0;
	bdesc->revision[0] = 0;
#endif
*/ 
/*
#if !defined(CONFIG_DM_MMC) && (!defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_LIBDISK_SUPPORT))
	part_init(bdesc);
#endif
*/
	return 0;
}


static int sd_send_op_cond(struct mmc *mmc, bool uhs_en)
{
	int timeout = 1000;
	int err;
	struct mmc_cmd cmd;

	while (1) {
		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;

		/*
		 * Most cards do not answer if some reserved bits
		 * in the ocr are set. However, Some controller
		 * can set bit 7 (reserved for low voltages), but
		 * how to manage low voltages SD card is not yet
		 * specified.
		 */
		cmd.cmdarg = 0/*mmc_host_is_spi(mmc)*/ ? 0 :
			(mmc->cfg->voltages & 0xff8000);

		if (mmc->version == SD_VERSION_2)
			cmd.cmdarg |= OCR_HCS;

		if (uhs_en)
			cmd.cmdarg |= OCR_S18R;

		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;

		if (cmd.response[0] & OCR_BUSY)
			break;

		if (timeout-- <= 0)
			return -1;

		udelay(1000);
	}

	if (mmc->version != SD_VERSION_2)
		mmc->version = SD_VERSION_1_0;

	if (0/*mmc_host_is_spi(mmc)*/) { /* read OCR for spi */
		cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;
	}

	mmc->ocr = cmd.response[0];
/*
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	if (uhs_en && !(mmc_host_is_spi(mmc)) && (cmd.response[0] & 0x41000000)
	    == 0x41000000) {
		err = mmc_switch_voltage(mmc, MMC_SIGNAL_VOLTAGE_180);
		if (err)
			return err;
	}
#endif
*/
	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 0;

	return 0;
}

static int mmc_send_if_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int err;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	/* We set the bit if the host supports voltages between 2.7 and 3.6 V */
	cmd.cmdarg = ((mmc->cfg->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	if ((cmd.response[0] & 0xff) != 0xaa)
		return -1;
	else
		mmc->version = SD_VERSION_2;

	return 0;
}

static int mmc_send_op_cond_iter(struct mmc *mmc, int use_arg)
{
	struct mmc_cmd cmd;
	int err;

	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
	cmd.resp_type = MMC_RSP_R3;
	cmd.cmdarg = 0;
	if (use_arg /*&& !mmc_host_is_spi(mmc)*/)
		cmd.cmdarg = OCR_HCS |
			(mmc->cfg->voltages &
			(mmc->ocr & OCR_VOLTAGE_MASK)) |
			(mmc->ocr & OCR_ACCESS_MODE);

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;
	mmc->ocr = cmd.response[0];
	return 0;
}

static int mmc_send_op_cond(struct mmc *mmc)
{
	int err, i;
	int timeout = 1000;
	uint32 start;

	/* Some cards seem to need this */
	mmc_go_idle(mmc);

	start = timer_get_count();
 	/* Asking to the card its capabilities */
	for (i = 0; ; i++) {
		err = mmc_send_op_cond_iter(mmc, i != 0);
		if (err)
			return err;

		/* exit if not busy (flag seems to be inverted) */
		if (mmc->ocr & OCR_BUSY)
			break;

		if (timer_get_count() - start > timeout)
			return -1;
		udelay(100);
	}
	mmc->op_cond_pending = 1;
	return 0;
}

static int mmc_complete_op_cond(struct mmc *mmc)
{
	struct mmc_cmd cmd;
	int timeout = 1000;
	uint64 start;
	int err;

	mmc->op_cond_pending = 0;
	if (!(mmc->ocr & OCR_BUSY)) {
		/* Some cards seem to need this */
		mmc_go_idle(mmc);

		start = timer_get_count();
		while (1) {
			err = mmc_send_op_cond_iter(mmc, 1);
			if (err)
				return err;
			if (mmc->ocr & OCR_BUSY)
				break;
			if (timer_get_count()-start > timeout)
				return -1;
			udelay(100);
		}
	}

	if (0/*mmc_host_is_spi(mmc)*/) { /* read OCR for spi */
		cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = 0;

		err = mmc_send_cmd(mmc, &cmd, NULL);

		if (err)
			return err;

		mmc->ocr = cmd.response[0];
	}

	mmc->version = MMC_VERSION_UNKNOWN;

	mmc->high_capacity = ((mmc->ocr & OCR_HCS) == OCR_HCS);
	mmc->rca = 1;

	return 0;
}


/*
 * put the host in the initial state:
 * - turn on Vdd (card power supply)
 * - configure the bus width and clock to minimal values
 */
static void mmc_set_initial_state(struct mmc *mmc)
{
	int err;

	/* First try to set 3.3V. If it fails set to 1.8V */
//	err = mmc_set_signal_voltage(mmc, MMC_SIGNAL_VOLTAGE_330);
//	if (err != 0)
//		err = mmc_set_signal_voltage(mmc, MMC_SIGNAL_VOLTAGE_180);
//	if (err != 0)
//		pr_warn("mmc: failed to set signal voltage\n");

	mmc_select_mode(mmc, MMC_LEGACY);
	mmc_set_bus_width(mmc, 1);
	mmc_set_clock(mmc, 0, MMC_CLK_ENABLE);
}

static int mmc_power_cycle(struct mmc *mmc)
{
	int ret;
/*
	ret = mmc_power_off(mmc);
	if (ret)
		return ret;

	ret = mmc_host_power_cycle(mmc);
	if (ret)
		return ret;
*/
	/*
	 * SD spec recommends at least 1ms of delay. Let's wait for 2ms
	 * to be on the safer side.
	 */
//	udelay(2000);
//	return mmc_power_on(mmc);
}

static int mmc_power_init(struct mmc *mmc)
{
    // It should power_up controller
    // I have it on after U-Boot
    // skip for now

	return 0;
}


int mmc_get_op_cond(struct mmc *mmc, bool quiet) {

    bool uhs_en = false; /*NOT FOR NOW   supports_uhs(mmc->cfg->host_caps);*/
	int err;

    if (mmc->has_init)
		return 0;
    
    err = mmc_power_init(mmc);
	if (err)
		return err;

// call mmc_init from device driver (I have 1 only now)
    err = dw_mmc_init(mmc);

	if (err)
		return err;
	mmc->ddr_mode = 0;

retry:
	mmc_set_initial_state(mmc);

	/* Reset the Card */
	err = mmc_go_idle(mmc);

	if (err)
		return err;

	/* The internal partition reset to user partition(0) at every CMD0 */
//	mmc_get_blk_desc(mmc)->hwpart = 0;

	/* Test for SD version 2 */
	err = mmc_send_if_cond(mmc);

	/* Now try to get the SD card's operating condition */
	err = sd_send_op_cond(mmc, uhs_en);
	if (err && uhs_en) {
		uhs_en = false;
		mmc_power_cycle(mmc);
		goto retry;
	}

	/* If the command timed out, we check for an MMC card */
	if (err == -1) {
		err = mmc_send_op_cond(mmc);

		if (err) {
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_LIBCOMMON_SUPPORT)
			if (!quiet)
				printf("Card did not respond to voltage select! : %d\n", err);
#endif
			return -1;
		}
	}

	return err;
}


int mmc_init(void) {
    struct mmc *mmc = &mmc0;

 	bool no_card;
	int err = 0;

	/*
	 * all hosts are capable of 1 bit bus-width and able to use the legacy
	 * timings.
	 */
	mmc->host_caps = mmc->cfg->host_caps | MMC_CAP(MMC_LEGACY) |
			 MMC_MODE_1BIT;   

    err = mmc_get_op_cond(mmc, false);

	if (!err)
		mmc->init_in_progress = 1;

    	mmc->init_in_progress = 0;
	if (mmc->op_cond_pending)
		err = mmc_complete_op_cond(mmc);

	if (!err)
		err = mmc_startup(mmc);
	if (err)
		mmc->has_init = 0;
	else
		mmc->has_init = 1;

	return err;
}