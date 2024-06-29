/* Copyright (c) 2018 SiFive, Inc */
/* SPDX-License-Identifier: Apache-2.0 */
/* SPDX-License-Identifier: GPL-2.0-or-later */
/* See the file LICENSE for further information */

#ifndef _DRIVERS_SPI_H
#define _DRIVERS_SPI_H

/* Register offsets */

#define SPI_REG_SCKDIV          0x00
#define SPI_REG_SCKMODE         0x04
#define SPI_REG_CSID            0x10
#define SPI_REG_CSDEF           0x14
#define SPI_REG_CSMODE          0x18

#define SPI_REG_DCSSCK          0x28
#define SPI_REG_DSCKCS          0x2a
#define SPI_REG_DINTERCS        0x2c
#define SPI_REG_DINTERXFR       0x2e

#define SPI_REG_FMT             0x40
#define SPI_REG_TXFIFO          0x48
#define SPI_REG_RXFIFO          0x4c
#define SPI_REG_TXCTRL          0x50
#define SPI_REG_RXCTRL          0x54

#define SPI_REG_FCTRL           0x60
#define SPI_REG_FFMT            0x64

#define SPI_REG_IE              0x70
#define SPI_REG_IP              0x74

/* Fields */

#define SPI_SCK_PHA             0x1
#define SPI_SCK_POL             0x2

#define SPI_FMT_PROTO(x)        ((x) & 0x3)
#define SPI_FMT_ENDIAN(x)       (((x) & 0x1) << 2)
#define SPI_FMT_DIR(x)          (((x) & 0x1) << 3)
#define SPI_FMT_LEN(x)          (((x) & 0xf) << 16)

/* TXCTRL register */
#define SPI_TXWM(x)             ((x) & 0xffff)
/* RXCTRL register */
#define SPI_RXWM(x)             ((x) & 0xffff)

#define SPI_IP_TXWM             0x1
#define SPI_IP_RXWM             0x2

#define SPI_FCTRL_EN            0x1

#define SPI_INSN_CMD_EN         0x1
#define SPI_INSN_ADDR_LEN(x)    (((x) & 0x7) << 1)
#define SPI_INSN_PAD_CNT(x)     (((x) & 0xf) << 4)
#define SPI_INSN_CMD_PROTO(x)   (((x) & 0x3) << 8)
#define SPI_INSN_ADDR_PROTO(x)  (((x) & 0x3) << 10)
#define SPI_INSN_DATA_PROTO(x)  (((x) & 0x3) << 12)
#define SPI_INSN_CMD_CODE(x)    (((x) & 0xff) << 16)
#define SPI_INSN_PAD_CODE(x)    (((x) & 0xff) << 24)

#define SPI_TXFIFO_FULL  (1 << 31)
#define SPI_RXFIFO_EMPTY (1 << 31)

/* Values */

#define SPI_CSMODE_AUTO         0
#define SPI_CSMODE_HOLD         2
#define SPI_CSMODE_OFF          3

#define SPI_DIR_RX              0
#define SPI_DIR_TX              1

#define SPI_PROTO_S             0
#define SPI_PROTO_D             1
#define SPI_PROTO_Q             2

#define SPI_ENDIAN_MSB          0
#define SPI_ENDIAN_LSB          1

#ifndef __ASSEMBLER__

#include <stdint.h>

#define _ASSERT_SIZEOF(type, size) _Static_assert(sizeof(type) == (size), #type " must be " #size " bytes wide")

typedef union
{
  struct
  {
    uint32_t pha : 1;
    uint32_t pol : 1;
    uint32_t reserved : 30;
  };
  uint32_t raw_bits;
} spi_reg_sckmode;
_ASSERT_SIZEOF(spi_reg_sckmode, 4);


typedef union
{
  struct
  {
    uint32_t mode : 2;
    uint32_t reserved : 30;
  };
  uint32_t raw_bits;
} spi_reg_csmode;
_ASSERT_SIZEOF(spi_reg_csmode, 4);


typedef union
{
  struct
  {
    uint32_t cssck : 8;
    uint32_t reserved0 : 8;
    uint32_t sckcs : 8;
    uint32_t reserved1 : 8;
  };
  uint32_t raw_bits;
} spi_reg_delay0;
_ASSERT_SIZEOF(spi_reg_delay0, 4);


typedef union
{
  struct
  {
    uint32_t intercs : 8;
    uint32_t reserved0 : 8;
    uint32_t interxfr : 8;
    uint32_t reserved1 : 8;
  };
  uint32_t raw_bits;
} spi_reg_delay1;
_ASSERT_SIZEOF(spi_reg_delay1, 4);


typedef union
{
  struct
  {
    uint32_t proto : 2;
    uint32_t endian : 1;
    uint32_t dir : 1;
    uint32_t reserved0 : 12;
    uint32_t len : 4;
    uint32_t reserved1 : 12;
  };
  uint32_t raw_bits;
} spi_reg_fmt;
_ASSERT_SIZEOF(spi_reg_fmt, 4);


typedef union
{
  struct
  {
    uint32_t data : 8;
    uint32_t reserved : 23;
    uint32_t full : 1;
  };
  uint32_t raw_bits;
} spi_reg_txdata;
_ASSERT_SIZEOF(spi_reg_txdata, 4);


typedef union
{
  struct
  {
    uint32_t data : 8;
    uint32_t reserved : 23;
    uint32_t empty : 1;
  };
  uint32_t raw_bits;
} spi_reg_rxdata;
_ASSERT_SIZEOF(spi_reg_rxdata, 4);


typedef union
{
  struct
  {
    uint32_t txmark : 3;
    uint32_t reserved : 29;
  };
  uint32_t raw_bits;
} spi_reg_txmark;
_ASSERT_SIZEOF(spi_reg_txmark, 4);


typedef union
{
  struct
  {
    uint32_t rxmark : 3;
    uint32_t reserved : 29;
  };
  uint32_t raw_bits;
} spi_reg_rxmark;
_ASSERT_SIZEOF(spi_reg_rxmark, 4);


typedef union
{
  struct
  {
    uint32_t en : 1;
    uint32_t reserved : 31;
  };
  uint32_t raw_bits;
} spi_reg_fctrl;
_ASSERT_SIZEOF(spi_reg_fctrl, 4);


typedef union
{
  struct
  {
    uint32_t cmd_en : 1;
    uint32_t addr_len : 3;
    uint32_t pad_cnt : 4;
    uint32_t command_proto : 2;
    uint32_t addr_proto : 2;
    uint32_t data_proto : 2;
    uint32_t reserved : 2;
    uint32_t command_code : 8;
    uint32_t pad_code : 8;
  };
  uint32_t raw_bits;
} spi_reg_ffmt;
_ASSERT_SIZEOF(spi_reg_ffmt, 4);


typedef union
{
  struct
  {
    uint32_t txwm : 1;
    uint32_t rxwm : 1;
    uint32_t reserved : 30;
  };
  uint32_t raw_bits;
} spi_reg_ie;
typedef spi_reg_ie spi_reg_ip;
_ASSERT_SIZEOF(spi_reg_ie, 4);
_ASSERT_SIZEOF(spi_reg_ip, 4);

#undef _ASSERT_SIZEOF


/**
 * SPI control register memory map.
 *
 * All functions take a pointer to a SPI device's control registers.
 */
typedef volatile struct
{
  uint32_t sckdiv;
  spi_reg_sckmode sckmode;
  uint32_t reserved08;
  uint32_t reserved0c;

  uint32_t csid;
  uint32_t csdef;
  spi_reg_csmode csmode;
  uint32_t reserved1c;

  uint32_t reserved20;
  uint32_t reserved24;
  spi_reg_delay0 delay0;
  spi_reg_delay1 delay1;

  uint32_t reserved30;
  uint32_t reserved34;
  uint32_t reserved38;
  uint32_t reserved3c;

  spi_reg_fmt fmt;
  uint32_t reserved44;
  spi_reg_txdata txdata;
  spi_reg_rxdata rxdata;

  spi_reg_txmark txmark;
  spi_reg_rxmark rxmark;
  uint32_t reserved58;
  uint32_t reserved5c;

  spi_reg_fctrl fctrl;
  spi_reg_ffmt ffmt;
  uint32_t reserved68;
  uint32_t reserved6c;

  spi_reg_ie ie;
  spi_reg_ip ip;
} spi_ctrl;


void spi_tx(spi_ctrl* spictrl, uint8_t in);
uint8_t spi_rx(spi_ctrl* spictrl);
uint8_t spi_txrx(spi_ctrl* spictrl, uint8_t in);
int spi_copy(spi_ctrl* spictrl, void* buf, uint32_t addr, uint32_t size);


// Inlining header functions in C
// https://stackoverflow.com/a/23699777/7433423

/**
 * Get smallest clock divisor that divides input_khz to a quotient less than or
 * equal to max_target_khz;
 */
inline unsigned int spi_min_clk_divisor(unsigned int input_khz, unsigned int max_target_khz)
{
  // f_sck = f_in / (2 * (div + 1)) => div = (f_in / (2*f_sck)) - 1
  //
  // The nearest integer solution for div requires rounding up as to not exceed
  // max_target_khz.
  //
  // div = ceil(f_in / (2*f_sck)) - 1
  //     = floor((f_in - 1 + 2*f_sck) / (2*f_sck)) - 1
  //
  // This should not overflow as long as (f_in - 1 + 2*f_sck) does not exceed
  // 2^32 - 1, which is unlikely since we represent frequencies in kHz.
  unsigned int quotient = (input_khz + 2 * max_target_khz - 1) / (2 * max_target_khz);
  // Avoid underflow
  if (quotient == 0) {
    return 0;
  } else {
    return quotient - 1;
  }
}

#endif /* !__ASSEMBLER__ */

#endif /* _DRIVERS_SPI_H */