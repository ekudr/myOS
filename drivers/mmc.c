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
#include <linux/errno.h>
#include <mmc.h>
#include <part.h>

#define __swab32(x) \
	((__u32)( \
		(((__u32)(x) & (__u32)0x000000ffUL) << 24) | \
		(((__u32)(x) & (__u32)0x0000ff00UL) <<  8) | \
		(((__u32)(x) & (__u32)0x00ff0000UL) >>  8) | \
		(((__u32)(x) & (__u32)0xff000000UL) >> 24) ))

#define __be32_to_cpu(x) __swab32((__u32)(x))

int dw_send_cmd(struct mmc *mmc, struct mmc_cmd *cmd, struct mmc_data *data);
int dw_set_ios(struct mmc *mmc);
int dw_mmc_init(struct mmc *mmc);

extern disk_t boot_disk;

// supposted we have 1 MMC dev for now
mmc_t mmc0;
struct mmc_config mmc_cfg0;

void 
mmmc_trace_before_send(mmc_t *mmc, struct mmc_cmd *cmd) {
	printf("[MMC] CMD_SEND:%d\n", cmd->cmdidx);
	printf("[MMC] \t\tARG\t\t\t 0x%08x\n", cmd->cmdarg);
}

void 
mmmc_trace_after_send(mmc_t *mmc, struct mmc_cmd *cmd, int ret) {
	int i;
	uint8_t *ptr;

	if (ret) {
		printf("[MMC] \t\tRET\t\t\t %d\n", ret);
	} else {
		switch (cmd->resp_type) {
		case MMC_RSP_NONE:
			printf("[MMC] \t\tMMC_RSP_NONE\n");
			break;
		case MMC_RSP_R1:
			printf("[MMC] \t\tMMC_RSP_R1,5,6,7 \t 0x%08x \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R1b:
			printf("[MMC] \t\tMMC_RSP_R1b\t\t 0x%08x \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R2:
			printf("[MMC] \t\tMMC_RSP_R2\t\t 0x%08x \n",
				cmd->response[0]);
			printf("[MMC] \t\t          \t\t 0x%08x \n",
				cmd->response[1]);
			printf("[MMC] \t\t          \t\t 0x%08x \n",
				cmd->response[2]);
			printf("[MMC] \t\t          \t\t 0x%08x \n",
				cmd->response[3]);
			printf("[MMC] \n");
			printf("[MMC] \t\t\t\t\tDUMPING DATA\n");
			for (i = 0; i < 4; i++) {
				int j;
				printf("[MMC] \t\t\t\t\t%03d - ", i*4);
				ptr = (uint8_t *)&cmd->response[i];
				ptr += 3;
				for (j = 0; j < 4; j++)
					printf("%02x ", *ptr--);
				printf("\n");
			}
			break;
		case MMC_RSP_R3:
			printf("[MMC] \t\tMMC_RSP_R3,4\t\t 0x%08x \n",
				cmd->response[0]);
			break;
		default:
			printf("[MMC] \t\tERROR MMC rsp not supported\n");
			break;
		}
	}
}

void 
mmc_trace_state(mmc_t *mmc, struct mmc_cmd *cmd) {
	int status;

	status = (cmd->response[0] & MMC_STATUS_CURR_STATE) >> 9;
	printf("[MMC] CURR STATE:%d\n", status);
}

const char *
mmc_mode_name(enum bus_mode mode) {

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

static uint32 
mmc_mode2freq(mmc_t *mmc, enum bus_mode mode) {

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

int 
mmc_send_cmd(mmc_t *mmc, struct mmc_cmd *cmd, struct mmc_data *data) {

	int ret;

	mmmc_trace_before_send(mmc, cmd);
//	ret = mmc->cfg->ops->send_cmd(mmc, cmd, data);
    ret = dw_send_cmd(mmc, cmd, data);
	mmmc_trace_after_send(mmc, cmd, ret);

	return ret;
}


/**
 * mmc_send_cmd_retry() - send a command to the mmc device, retrying on error
 *
 * @dev:	device to receive the command
 * @cmd:	command to send
 * @data:	additional data to send/receive
 * @retries:	how many times to retry; mmc_send_cmd is always called at least
 *              once
 * @return 0 if ok, -ve on error
 */
static int 
mmc_send_cmd_retry(mmc_t *mmc, struct mmc_cmd *cmd,
			      struct mmc_data *data, uint32_t retries) {
	int ret;

	do {
		ret = mmc_send_cmd(mmc, cmd, data);
	} while (ret && retries--);

	return ret;
}

/**
 * mmc_send_cmd_quirks() - send a command to the mmc device, retrying if a
 *                         specific quirk is enabled
 *
 * @dev:	device to receive the command
 * @cmd:	command to send
 * @data:	additional data to send/receive
 * @quirk:	retry only if this quirk is enabled
 * @retries:	how many times to retry; mmc_send_cmd is always called at least
 *              once
 * @return 0 if ok, -ve on error
 */
static int 
mmc_send_cmd_quirks(mmc_t *mmc, struct mmc_cmd *cmd,
			       struct mmc_data *data, uint32_t quirk, uint32_t retries) {
//	if (CONFIG_IS_ENABLED(MMC_QUIRKS) && mmc->quirks & quirk)
//		return mmc_send_cmd_retry(mmc, cmd, data, retries);
//	else
		return mmc_send_cmd(mmc, cmd, data);
}



static int sd_switch(mmc_t *mmc, int mode, int group, uint8_t value, uint8_t *resp)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	/* Switch the frequency */
	cmd.cmdidx = SD_CMD_SWITCH_FUNC;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = (mode << 31) | 0xffffff;
	cmd.cmdarg &= ~(0xf << (group * 4));
	cmd.cmdarg |= value << (group * 4);

	data.dest = (char *)resp;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	return mmc_send_cmd(mmc, &cmd, &data);
}

static int sd_get_capabilities(mmc_t *mmc)
{
	int err;
	struct mmc_cmd cmd;
    //rewrite allocation
    char __scr[16] _ALIGN(32); // __be32[2]
    _ALIGN(32) uint32_t *scr =  (uint32_t*)__scr; 
	char __switch_status[128] _ALIGN(32);   // __be32[16]
    _ALIGN(32) uint32_t *switch_status = (uint32_t*)__switch_status;
	struct mmc_data data;
	int timeout;
/*    
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	u32 sd3_bus_mode;
#endif
*/
	mmc->card_caps = MMC_MODE_1BIT | MMC_CAP(MMC_LEGACY);

	if (mmc_host_is_spi(mmc))
		return 0;

	/* Read the SCR to find out if this card supports higher speeds */
	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;

	err = mmc_send_cmd(mmc, &cmd, NULL);

	if (err)
		return err;

	cmd.cmdidx = SD_CMD_APP_SEND_SCR;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.dest = (char *)scr;
	data.blocksize = 8;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd_retry(mmc, &cmd, &data, 3);

	if (err)
		return err;

	mmc->scr[0] = __be32_to_cpu(scr[0]);
	mmc->scr[1] = __be32_to_cpu(scr[1]);

	switch ((mmc->scr[0] >> 24) & 0xf) {
	case 0:
		mmc->version = SD_VERSION_1_0;
		break;
	case 1:
		mmc->version = SD_VERSION_1_10;
		break;
	case 2:
		mmc->version = SD_VERSION_2;
		if ((mmc->scr[0] >> 15) & 0x1)
			mmc->version = SD_VERSION_3;
		break;
	default:
		mmc->version = SD_VERSION_1_0;
		break;
	}

	if (mmc->scr[0] & SD_DATA_4BIT)
		mmc->card_caps |= MMC_MODE_4BIT;

	/* Version 1.0 doesn't support switching */
	if (mmc->version == SD_VERSION_1_0)
		return 0;

	timeout = 4;
	while (timeout--) {
		err = sd_switch(mmc, SD_SWITCH_CHECK, 0, 1,
				(uint8_t *)switch_status);

		if (err)
			return err;

		/* The high-speed function is busy.  Try again */
		if (!(__be32_to_cpu(switch_status[7]) & SD_HIGHSPEED_BUSY))
			break;
	}

	/* If high-speed isn't supported, we return */
	if (__be32_to_cpu(switch_status[3]) & SD_HIGHSPEED_SUPPORTED)
		mmc->card_caps |= MMC_CAP(SD_HS);
/*
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	// Version before 3.0 don't support UHS modes 
	if (mmc->version < SD_VERSION_3)
		return 0;

	sd3_bus_mode = __be32_to_cpu(switch_status[3]) >> 16 & 0x1f;
	if (sd3_bus_mode & SD_MODE_UHS_SDR104)
		mmc->card_caps |= MMC_CAP(UHS_SDR104);
	if (sd3_bus_mode & SD_MODE_UHS_SDR50)
		mmc->card_caps |= MMC_CAP(UHS_SDR50);
	if (sd3_bus_mode & SD_MODE_UHS_SDR25)
		mmc->card_caps |= MMC_CAP(UHS_SDR25);
	if (sd3_bus_mode & SD_MODE_UHS_SDR12)
		mmc->card_caps |= MMC_CAP(UHS_SDR12);
	if (sd3_bus_mode & SD_MODE_UHS_DDR50)
		mmc->card_caps |= MMC_CAP(UHS_DDR50);
#endif
*/
	return 0;
}

static int 
sd_set_card_speed(mmc_t *mmc, enum bus_mode mode) {
	int err;

	char __switch_status[128] _ALIGN(32);   // __be32[16]
    _ALIGN(32) uint32_t *switch_status = (uint32_t*)__switch_status;

	int speed;

	/* SD version 1.00 and 1.01 does not support CMD 6 */
	if (mmc->version == SD_VERSION_1_0)
		return 0;

	switch (mode) {
	case MMC_LEGACY:
		speed = UHS_SDR12_BUS_SPEED;
		break;
	case SD_HS:
		speed = HIGH_SPEED_BUS_SPEED;
		break;
/*       
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	case UHS_SDR12:
		speed = UHS_SDR12_BUS_SPEED;
		break;
	case UHS_SDR25:
		speed = UHS_SDR25_BUS_SPEED;
		break;
	case UHS_SDR50:
		speed = UHS_SDR50_BUS_SPEED;
		break;
	case UHS_DDR50:
		speed = UHS_DDR50_BUS_SPEED;
		break;
	case UHS_SDR104:
		speed = UHS_SDR104_BUS_SPEED;
		break;
#endif
*/ 
	default:
		return -EINVAL;
	}

	err = sd_switch(mmc, SD_SWITCH_SWITCH, 0, speed, (uint8_t *)switch_status);
	if (err)
		return err;

	if (((__be32_to_cpu(switch_status[4]) >> 24) & 0xF) != speed)
		return -ENOTSUPP;

	return 0;
}

static int 
sd_select_bus_width(mmc_t *mmc, int w) {
	int err;
	struct mmc_cmd cmd;

	if ((w != 4) && (w != 1))
		return -EINVAL;

	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;

	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
	cmd.resp_type = MMC_RSP_R1;
	if (w == 4)
		cmd.cmdarg = 2;
	else if (w == 1)
		cmd.cmdarg = 0;
	err = mmc_send_cmd(mmc, &cmd, NULL);
	if (err)
		return err;

	return 0;
}

/*
#if CONFIG_IS_ENABLED(MMC_WRITE)
static int sd_read_ssr(struct mmc *mmc)
{
	static const unsigned int sd_au_size[] = {
		0,		SZ_16K / 512,		SZ_32K / 512,
		SZ_64K / 512,	SZ_128K / 512,		SZ_256K / 512,
		SZ_512K / 512,	SZ_1M / 512,		SZ_2M / 512,
		SZ_4M / 512,	SZ_8M / 512,		(SZ_8M + SZ_4M) / 512,
		SZ_16M / 512,	(SZ_16M + SZ_8M) / 512,	SZ_32M / 512,
		SZ_64M / 512,
	};
	int err, i;
	struct mmc_cmd cmd;
	ALLOC_CACHE_ALIGN_BUFFER(uint, ssr, 16);
	struct mmc_data data;
	unsigned int au, eo, et, es;

	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = mmc->rca << 16;

	err = mmc_send_cmd_quirks(mmc, &cmd, NULL, MMC_QUIRK_RETRY_APP_CMD, 4);
	if (err)
		return err;

	cmd.cmdidx = SD_CMD_APP_SD_STATUS;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.dest = (char *)ssr;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd_retry(mmc, &cmd, &data, 3);
	if (err)
		return err;

	for (i = 0; i < 16; i++)
		ssr[i] = be32_to_cpu(ssr[i]);

	au = (ssr[2] >> 12) & 0xF;
	if ((au <= 9) || (mmc->version == SD_VERSION_3)) {
		mmc->ssr.au = sd_au_size[au];
		es = (ssr[3] >> 24) & 0xFF;
		es |= (ssr[2] & 0xFF) << 8;
		et = (ssr[3] >> 18) & 0x3F;
		if (es && et) {
			eo = (ssr[3] >> 16) & 0x3;
			mmc->ssr.erase_timeout = (et * 1000) / es;
			mmc->ssr.erase_offset = eo * 1000;
		}
	} else {
		pr_debug("Invalid Allocation Unit Size.\n");
	}

	return 0;
}
*/
/*
 * helper function to display the capabilities in a human
 * friendly manner. The capabilities include bus width and
 * supported modes.
 */
void mmc_dump_capabilities(const char *text, uint32_t caps)
{
	enum bus_mode mode;

	printf("%s: widths [", text);
	if (caps & MMC_MODE_8BIT)
		printf("8, ");
	if (caps & MMC_MODE_4BIT)
		printf("4, ");
	if (caps & MMC_MODE_1BIT)
		printf("1, ");
	printf("\b\b] modes [");
	for (mode = MMC_LEGACY; mode < MMC_MODES_END; mode++)
		if (MMC_CAP(mode) & caps)
			printf("%s, ", mmc_mode_name(mode));
	printf("\b\b]\n");
}

/* frequency bases */
/* divided by 10 to be nice to platforms without floating point */
static const int fbase[] = {
	10000,
	100000,
	1000000,
	10000000,
};

/* Multiplier values for TRAN_SPEED.  Multiplied by 10 to be nice
 * to platforms without floating point.
 */
static const uint8_t multipliers[] = {
	0,	/* reserved */
	10,
	12,
	13,
	15,
	20,
	25,
	30,
	35,
	40,
	45,
	50,
	55,
	60,
	70,
	80,
};

static inline int bus_width(uint32_t cap)
{
	if (cap == MMC_MODE_8BIT)
		return 8;
	if (cap == MMC_MODE_4BIT)
		return 4;
	if (cap == MMC_MODE_1BIT)
		return 1;
	printf("[MMC] invalid bus witdh capability 0x%x\n", cap);
	return 0;
}

static int 
mmc_read_blocks(mmc_t *mmc, void *dst, uint64_t start,
			   uint64_t blkcnt) {
	struct mmc_cmd cmd;
	struct mmc_data data;

	if (blkcnt > 1)
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	else
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;

	if (mmc->high_capacity)
		cmd.cmdarg = start;
	else
		cmd.cmdarg = start * mmc->read_bl_len;

	cmd.resp_type = MMC_RSP_R1;

	data.dest = dst;
	data.blocks = blkcnt;
	data.blocksize = mmc->read_bl_len;
	data.flags = MMC_DATA_READ;

	if (mmc_send_cmd(mmc, &cmd, &data))
		return 0;

	if (blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (mmc_send_cmd(mmc, &cmd, NULL)) {
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_LIBCOMMON_SUPPORT)
			printf("mmc fail to send stop cmd\n");
#endif
			return 0;
		}
	}

	return blkcnt;
}


static int 
mmc_select_mode(mmc_t *mmc, enum bus_mode mode) {
	mmc->selected_mode = mode;
	mmc->tran_speed = mmc_mode2freq(mmc, mode);
	mmc->ddr_mode = mmc_is_mode_ddr(mode);
	printf("[MMC] selecting mode %s (freq : %d MHz)\n", mmc_mode_name(mode),
		 mmc->tran_speed / 1000000);
	return 0;
}

int 
mmc_set_clock(mmc_t *mmc, uint32_t clock, bool disable) {
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

static int 
mmc_set_bus_width(mmc_t *mmc, uint32_t width) {
	mmc->bus_width = width;
	return dw_set_ios(mmc);
}

struct mode_width_tuning {
	enum bus_mode mode;
	uint32_t widths;
#ifdef MMC_SUPPORTS_TUNING
	uint tuning;
#endif
};

static const struct mode_width_tuning sd_modes_by_pref[] = {
/*   
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
#ifdef MMC_SUPPORTS_TUNING
	{
		.mode = UHS_SDR104,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
		.tuning = MMC_CMD_SEND_TUNING_BLOCK
	},
#endif
	{
		.mode = UHS_SDR50,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
	{
		.mode = UHS_DDR50,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
	{
		.mode = UHS_SDR25,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
#endif
*/ 
	{
		.mode = SD_HS,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
/*
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
	{
		.mode = UHS_SDR12,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	},
#endif
*/
	{
		.mode = MMC_LEGACY,
		.widths = MMC_MODE_4BIT | MMC_MODE_1BIT,
	}
};


#define for_each_sd_mode_by_pref(caps, mwt)     \
	for (mwt = sd_modes_by_pref;        \
	     mwt < sd_modes_by_pref + ARRAY_SIZE(sd_modes_by_pref);     \
	     mwt++)         \
		if (caps & MMC_CAP(mwt->mode))



static int 
sd_select_mode_and_width(mmc_t *mmc, uint32_t card_caps) {
	int err;
	uint32_t widths[] = {MMC_MODE_4BIT, MMC_MODE_1BIT};
	const struct mode_width_tuning *mwt;
   
//#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT)
//	bool uhs_en = (mmc->ocr & OCR_S18R) ? true : false;
//#else
	bool uhs_en = false;
//#endif
	uint32_t caps;

//#ifdef DEBUG
	mmc_dump_capabilities("sd card", card_caps);
	mmc_dump_capabilities("host", mmc->host_caps);
//#endif

	if (mmc_host_is_spi(mmc)) {
		mmc_set_bus_width(mmc, 1);
		mmc_select_mode(mmc, MMC_LEGACY);
		mmc_set_clock(mmc, mmc->tran_speed, MMC_CLK_ENABLE);
/*        
#if CONFIG_IS_ENABLED(MMC_WRITE)
		err = sd_read_ssr(mmc);
		if (err)
			pr_warn("unable to read ssr\n");
#endif
*/
		return 0;
	}

	/* Restrict card's capabilities by what the host can do */
	caps = card_caps & mmc->host_caps;

//	if (!uhs_en)
//		caps &= ~UHS_CAPS;

	for_each_sd_mode_by_pref(caps, mwt) {
		uint32_t *w;

		for (w = widths; w < widths + ARRAY_SIZE(widths); w++) {
			if (*w & caps & mwt->widths) {
				printf("[MMC] trying mode %s width %d (at %d MHz)\n",
					 mmc_mode_name(mwt->mode),
					 bus_width(*w),
					 mmc_mode2freq(mmc, mwt->mode) / 1000000);

				/* configure the bus width (card + host) */
				err = sd_select_bus_width(mmc, bus_width(*w));
				if (err)
					goto error;
				mmc_set_bus_width(mmc, bus_width(*w));

				/* configure the bus mode (card) */
				err = sd_set_card_speed(mmc, mwt->mode);
				if (err)
					goto error;

				/* configure the bus mode (host) */
				mmc_select_mode(mmc, mwt->mode);
				mmc_set_clock(mmc, mmc->tran_speed,
						MMC_CLK_ENABLE);

#ifdef MMC_SUPPORTS_TUNING
				/* execute tuning if needed */
				if (mwt->tuning && !mmc_host_is_spi(mmc)) {
					err = mmc_execute_tuning(mmc,
								 mwt->tuning);
					if (err) {
						pr_debug("tuning failed\n");
						goto error;
					}
				}
#endif
/*
#if CONFIG_IS_ENABLED(MMC_WRITE)
				err = sd_read_ssr(mmc);
				if (err)
					pr_warn("unable to read ssr\n");
#endif
*/
				if (!err)
					return 0;

error:
				/* revert to a safer bus speed */
				mmc_select_mode(mmc, MMC_LEGACY);
				mmc_set_clock(mmc, mmc->tran_speed,
						MMC_CLK_ENABLE);
			}
		}
	}

	printf("[MMC] unable to select a mode\n");
	return -ENOTSUPP;
}


static int 
mmc_go_idle(mmc_t *mmc)
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



static int 
mmc_startup(mmc_t *mmc) {
	int err, i;
	uint32_t mult, freq;
	uint64_t cmult, csize;
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

	cmd.cmdidx = mmc_host_is_spi(mmc) ? MMC_CMD_SEND_CID :
		MMC_CMD_ALL_SEND_CID; /* cmd not supported in spi */


	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;

	err = mmc_send_cmd_quirks(mmc, &cmd, NULL, MMC_QUIRK_RETRY_SEND_CID, 4);
	if (err)
		return err;

	sbi_memcpy(mmc->cid, cmd.response, 16);

	/*
	 * For MMC cards, set the Relative Address.
	 * For SD cards, get the Relatvie Address.
	 * This also puts the cards into Standby State
	 */

	if (!mmc_host_is_spi(mmc)) { /* cmd not supported in spi */
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

//    printf("[MMC] mmc version %X\n", mmc->version);

	/* divide frequency by 10, since the mults are 10x bigger */
	freq = fbase[(cmd.response[0] & 0x7)];
	mult = multipliers[((cmd.response[0] >> 3) & 0xf)];

	mmc->legacy_speed = freq * mult;
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
	if (!mmc_host_is_spi(mmc)) { /* cmd not supported in spi */
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

//	err = mmc_startup_v4(mmc);
//	if (err)
//		return err;
        
/*
	err = mmc_set_capacity(mmc, mmc_get_blk_desc(mmc)->hwpart);
	if (err)
		return err;
*/
#if 0 //CONFIG_IS_ENABLED(MMC_TINY)
	mmc_set_clock(mmc, mmc->legacy_speed, false);
	mmc_select_mode(mmc, MMC_LEGACY);
	mmc_set_bus_width(mmc, 1);
#else
	if (IS_SD(mmc)) {
		err = sd_get_capabilities(mmc);
		if (err)
			return err;

		err = sd_select_mode_and_width(mmc, mmc->card_caps);
	}/* else {
		err = mmc_get_capabilities(mmc);
		if (err)
			return err;
		err = mmc_select_mode_and_width(mmc, mmc->card_caps);
	}*/
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


static int 
sd_send_op_cond(mmc_t *mmc, bool uhs_en) {
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
		cmd.cmdarg = mmc_host_is_spi(mmc) ? 0 :
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
			return -ETIMEDOUT;

		udelay(1000);
	}

	if (mmc->version != SD_VERSION_2)
		mmc->version = SD_VERSION_1_0;

	if (mmc_host_is_spi(mmc)) { /* read OCR for spi */
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

static int 
mmc_send_if_cond(mmc_t *mmc) {
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
		return -EOPNOTSUPP;
	else
		mmc->version = SD_VERSION_2;

	return 0;
}

static int 
mmc_send_op_cond_iter(mmc_t *mmc, int use_arg) {
	struct mmc_cmd cmd;
	int err;

	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
	cmd.resp_type = MMC_RSP_R3;
	cmd.cmdarg = 0;
	if (use_arg && !mmc_host_is_spi(mmc))
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

static int 
mmc_send_op_cond(mmc_t *mmc) {
	int err, i;
	int timeout = 1000;
	uint64_t start;

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
			return -ETIMEDOUT;
		udelay(100);
	}
	mmc->op_cond_pending = 1;
	return 0;
}

static int 
mmc_complete_op_cond(mmc_t *mmc) {
	struct mmc_cmd cmd;
	int timeout = 1000;
	uint64_t start;
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
				return -ETIMEDOUT;
			udelay(100);
		}
	}

	if (mmc_host_is_spi(mmc)) { /* read OCR for spi */
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
static void 
mmc_set_initial_state(mmc_t *mmc) {
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

static int 
mmc_power_cycle(mmc_t *mmc) {
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

static int 
mmc_power_init(mmc_t *mmc) {
    // It should power_up controller
    // I have it on after U-Boot
    // skip for now

	return 0;
}


int 
mmc_get_op_cond(mmc_t *mmc, bool quiet) {

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
	if (err == -ETIMEDOUT) {
		err = mmc_send_op_cond(mmc);

		if (err) {
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_LIBCOMMON_SUPPORT)
			if (!quiet)
				printf("[MMC] Card did not respond to voltage select! : %d\n", err);
#endif
			return -EOPNOTSUPP;
		}
	}

	return err;
}

int mmc_bread(void* dst, uint32_t src_lba, size_t size) {
  if(mmc_read_blocks(&mmc0, dst, src_lba, size))
		return 0;
  return -1;
}

int mmc_init(void) {
    mmc_t *mmc = &mmc0;

    char *buf[512];
 	bool no_card;
	int err = 0;

	boot_disk.dev_inited = false;

    mmc->cfg = &mmc_cfg0;

    
    err = dw_set_plat(mmc);

    /*
	 * all hosts are capable of 1 bit bus-width and able to use the legacy
	 * timings.
	 */
	mmc->host_caps = mmc->cfg->host_caps | MMC_CAP(MMC_LEGACY) |
			 MMC_MODE_1BIT;   


    no_card = 0;  // dw_getcd(mmc);
    if (no_card) {
		mmc->has_init = 0;
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_LIBCOMMON_SUPPORT)
		printf("[MMC] no card present\n");
#endif
		return -ENOMEDIUM;
	}


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
/*
    err = mmc_read_blocks(mmc, buf, 1,1);

    printf("[SD_CARD] COPY read return %d\n", err);
 


for(int i=0; i<512/8; i++){
  printf("GPT: %d => 0x%X\n", i, buf[i]);
}
*/
	boot_disk.bread = mmc_bread;
	boot_disk.dev_inited = true;

	return err;
}
