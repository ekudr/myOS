#ifndef __DWMMC_H
#define __DWMMC_H

#define DWMCI_CTRL		0x000
#define	DWMCI_PWREN		0x004
#define DWMCI_CLKDIV		0x008
#define DWMCI_CLKSRC		0x00C
#define DWMCI_CLKENA		0x010
#define DWMCI_TMOUT		0x014
#define DWMCI_CTYPE		0x018
#define DWMCI_BLKSIZ		0x01C
#define DWMCI_BYTCNT		0x020
#define DWMCI_INTMASK		0x024
#define DWMCI_CMDARG		0x028
#define DWMCI_CMD		0x02C
#define DWMCI_RESP0		0x030
#define DWMCI_RESP1		0x034
#define DWMCI_RESP2		0x038
#define DWMCI_RESP3		0x03C
#define DWMCI_MINTSTS		0x040
#define DWMCI_RINTSTS		0x044
#define DWMCI_STATUS		0x048
#define DWMCI_FIFOTH		0x04C
#define DWMCI_CDETECT		0x050
#define DWMCI_WRTPRT		0x054
#define DWMCI_GPIO		0x058
#define DWMCI_TCMCNT		0x05C
#define DWMCI_TBBCNT		0x060
#define DWMCI_DEBNCE		0x064
#define DWMCI_USRID		0x068
#define DWMCI_VERID		0x06C
#define DWMCI_HCON		0x070
#define DWMCI_UHS_REG		0x074
#define DWMCI_BMOD		0x080
#define DWMCI_PLDMND		0x084
#define DWMCI_DBADDR		0x088
#define DWMCI_IDSTS		0x08C
#define DWMCI_IDINTEN		0x090
#define DWMCI_DSCADDR		0x094
#define DWMCI_BUFADDR		0x098
#define DWMCI_UHS_REG_EXT	0x108
#define DWMCI_DATA		0x200

/* Interrupt Mask register */
#define DWMCI_INTMSK_ALL	0xffffffff
#define DWMCI_INTMSK_RE		(1 << 1)
#define DWMCI_INTMSK_CDONE	(1 << 2)
#define DWMCI_INTMSK_DTO	(1 << 3)
#define DWMCI_INTMSK_TXDR	(1 << 4)
#define DWMCI_INTMSK_RXDR	(1 << 5)
#define DWMCI_INTMSK_RCRC	(1 << 6)
#define DWMCI_INTMSK_DCRC	(1 << 7)
#define DWMCI_INTMSK_RTO	(1 << 8)
#define DWMCI_INTMSK_DRTO	(1 << 9)
#define DWMCI_INTMSK_HTO	(1 << 10)
#define DWMCI_INTMSK_FRUN	(1 << 11)
#define DWMCI_INTMSK_HLE	(1 << 12)
#define DWMCI_INTMSK_SBE	(1 << 13)
#define DWMCI_INTMSK_ACD	(1 << 14)
#define DWMCI_INTMSK_EBE	(1 << 15)

/* Raw interrupt Regsiter */
#define DWMCI_DATA_ERR	(DWMCI_INTMSK_EBE | DWMCI_INTMSK_SBE | DWMCI_INTMSK_HLE |\
			DWMCI_INTMSK_FRUN | DWMCI_INTMSK_EBE | DWMCI_INTMSK_DCRC)
#define DWMCI_DATA_TOUT	(DWMCI_INTMSK_HTO | DWMCI_INTMSK_DRTO)
/* CTRL register */
#define DWMCI_CTRL_RESET	(1 << 0)
#define DWMCI_CTRL_FIFO_RESET	(1 << 1)
#define DWMCI_CTRL_DMA_RESET	(1 << 2)
#define DWMCI_DMA_EN		(1 << 5)
#define DWMCI_CTRL_SEND_AS_CCSD	(1 << 10)
#define DWMCI_IDMAC_EN		(1 << 25)
#define DWMCI_RESET_ALL		(DWMCI_CTRL_RESET | DWMCI_CTRL_FIFO_RESET |\
				DWMCI_CTRL_DMA_RESET)

/* CMD register */
#define DWMCI_CMD_RESP_EXP	(1 << 6)
#define DWMCI_CMD_RESP_LENGTH	(1 << 7)
#define DWMCI_CMD_CHECK_CRC	(1 << 8)
#define DWMCI_CMD_DATA_EXP	(1 << 9)
#define DWMCI_CMD_RW		(1 << 10)
#define DWMCI_CMD_SEND_STOP	(1 << 12)
#define DWMCI_CMD_ABORT_STOP	(1 << 14)
#define DWMCI_CMD_SEND_INIT	(1 << 15)
#define DWMCI_CMD_PRV_DAT_WAIT	(1 << 13)
#define DWMCI_CMD_UPD_CLK	(1 << 21)
#define DWMCI_CMD_USE_HOLD_REG	(1 << 29)
#define DWMCI_CMD_START		(1 << 31)

/* CLKENA register */
#define DWMCI_CLKEN_ENABLE	(1 << 0)
#define DWMCI_CLKEN_LOW_PWR	(1 << 16)

/* Card-type registe */
#define DWMCI_CTYPE_1BIT	0
#define DWMCI_CTYPE_4BIT	(1 << 0)
#define DWMCI_CTYPE_8BIT	(1 << 16)

/* Status Register */
#define DWMCI_FIFO_EMPTY	(1 << 2)
#define DWMCI_FIFO_FULL		(1 << 3)
#define DWMCI_BUSY		(1 << 9)
#define DWMCI_FIFO_MASK		0x1fff
#define DWMCI_FIFO_SHIFT	17

/* FIFOTH Register */
#define MSIZE(x)		((x) << 28)
#define RX_WMARK(x)		((x) << 16)
#define TX_WMARK(x)		(x)
#define RX_WMARK_SHIFT		16
#define RX_WMARK_MASK		(0xfff << RX_WMARK_SHIFT)

#define DWMCI_IDMAC_OWN		(1 << 31)
#define DWMCI_IDMAC_CH		(1 << 4)
#define DWMCI_IDMAC_FS		(1 << 3)
#define DWMCI_IDMAC_LD		(1 << 2)

/*  Bus Mode Register */
#define DWMCI_BMOD_IDMAC_RESET	(1 << 0)
#define DWMCI_BMOD_IDMAC_FB	(1 << 1)
#define DWMCI_BMOD_IDMAC_EN	(1 << 7)

/* UHS register */
#define DWMCI_DDR_MODE	(1 << 16)

/* Internal IDMAC interrupt defines */
#define DWMCI_IDINTEN_RI		BIT(1)
#define DWMCI_IDINTEN_TI		BIT(0)

#define DWMCI_IDINTEN_MASK	(DWMCI_IDINTEN_TI | \
				 DWMCI_IDINTEN_RI)

/* quirks */
#define DWMCI_QUIRK_DISABLE_SMU		(1 << 0)

typedef struct dwmmc
{
    void *ioaddr;
}dw_mmc;
 

#endif	/* __DWMMC_H */