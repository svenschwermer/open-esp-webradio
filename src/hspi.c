#include "hspi.h"
#include "common.h"
#include "espressif/esp8266/esp8266.h"
#include "espressif/esp_common.h"

struct spi_regs {
  uint32_t cmd;       // 0x00
  uint32_t addr;      // 0x04
  uint32_t ctrl;      // 0x08
  uint32_t ctrl1;     // 0x0c
  uint32_t rd_status; // 0x10
  uint32_t ctrl2;     // 0x14
  uint32_t clock;     // 0x18
  uint32_t user;      // 0x1c
  uint32_t user1;     // 0x20
  uint32_t user2;     // 0x24
  uint32_t wr_status; // 0x28
  uint32_t pin;       // 0x2c
  uint32_t slave;     // 0x30
  uint32_t slave1;    // 0x34
  uint32_t slave2;    // 0x38
  uint32_t slave3;    // 0x3c
  uint32_t w[16];     // 0x40..0x7c
  uint32_t _[28];     // 0x80..0xec
  uint32_t ext0;      // 0xf0
  uint32_t ext1;      // 0xf4
  uint32_t ext2;      // 0xf8
  uint32_t ext3;      // 0xfc
};

static void apply_settings(struct hspi *hspi, uint32_t user_reg);
static inline int min(int a, int b) { return (a < b) ? a : b; }

// The following SPI controller instances are located using the linker script.
extern volatile struct spi_regs SPI;  // aka SPI0, used for the flash mememry
extern volatile struct spi_regs HSPI; // aka SPI1

int hspi_init(struct hspi *settings) {
  switch (settings->mode) {
  case SPI_MODE_SPI:
  case SPI_MODE_DIO:
  case SPI_MODE_QIO:
    break;
  default:
    return 1;
  }

  switch (settings->cs) {
  case 0:
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_SD_CMD_U, FUNC_SPICS0);
    break;
  case 1:
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_SPICS1);
    break;
  case 2:
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_SPICS2);
    break;
  default:
    return 1;
  }

  if (settings->clock_div < 1 || settings->clock_div > 64)
    return 1;

  // hspi overlap to spi, two spi masters on cspi
  SET_PERI_REG_MASK(HOST_INF_SEL, PERI_IO_CSPI_OVERLAP);

  // set higher priority for spi than hspi
  SPI.ext3 |= 0x1;
  HSPI.ext3 |= 0x3;

  return 0;
}

size_t hspi_read(struct hspi *settings, size_t len, void *data, int addr_bits,
                 uint32_t addr, int cmd_bits, uint16_t cmd, int dummy_cycles) {
  // we can read a maximum of 16*4=64 bytes at a time
  if (len > 64)
    len = 64;

  uint32_t user_reg = SPI_CS_SETUP | SPI_CS_HOLD | SPI_USR_MISO | SPI_CK_I_EDGE;
  uint32_t user1_reg = ((8 * len) - 1) << SPI_USR_MISO_BITLEN_S;
  if (cmd_bits > 0)
    user_reg |= SPI_USR_COMMAND;
  if (addr_bits > 0) {
    user_reg |= SPI_USR_ADDR;
    user1_reg |= (min(addr_bits, 32) - 1) << SPI_USR_ADDR_BITLEN_S;
  }
  if (dummy_cycles > 0) {
    user_reg |= SPI_USR_DUMMY;
    user1_reg |= (min(dummy_cycles, 256) - 1) << SPI_USR_DUMMY_CYCLELEN_S;
  }

  // make sure the last operation is done
  while (HSPI.cmd & SPI_USR)
    ;

  apply_settings(settings, user_reg);

  HSPI.user1 = user1_reg;
  if (cmd_bits > 0)
    HSPI.user2 = ((min(cmd_bits, 16) - 1) << SPI_USR_COMMAND_BITLEN_S) | cmd;
  if (addr_bits > 0)
    HSPI.addr = addr << (32 - addr_bits);

  // start data transfer and wait until completion
  HSPI.cmd |= SPI_USR;
  while (HSPI.cmd & SPI_USR)
    ;

  if (((uintptr_t)data) & 0x3 || len % 4 != 0) {
    uint8_t *byte_buf = data;
    for (size_t i = 0; i < len;) {
      uint32_t d = HSPI.w[i / 4];
      byte_buf[i++] = d & 0xff;
      if (i < len)
        byte_buf[i++] = (d >> 8) & 0xff;
      if (i < len)
        byte_buf[i++] = (d >> 16) & 0xff;
      if (i < len)
        byte_buf[i++] = (d >> 24) & 0xff;
    }
  } else {
    uint32_t *word_buf = data;
    for (size_t i = 0; i < len / 4; ++i)
      word_buf[i] = HSPI.w[i];
  }

  return len;
}

size_t hspi_write(struct hspi *settings, size_t len, const void *data,
                  int addr_bits, uint32_t addr, int cmd_bits, uint16_t cmd) {
  // we can write a maximum of 16*4=64 bytes at a time
  if (len > 64)
    len = 64;
  if (addr_bits > 32)
    addr_bits = 32;
  if (cmd_bits > 16)
    cmd_bits = 16;

  uint32_t user_reg = SPI_CS_SETUP | SPI_CS_HOLD | SPI_CK_I_EDGE;
  if (cmd_bits > 0)
    user_reg |= SPI_USR_COMMAND;
  if (len > 0)
    user_reg |= SPI_USR_MOSI;
  if (addr_bits > 0)
    user_reg |= SPI_USR_ADDR;

  // make sure the last operation is done
  while (HSPI.cmd & SPI_USR)
    ;

  apply_settings(settings, user_reg);

  if (addr_bits > 0 || len > 0)
    HSPI.user1 = (((8 * len) - 1) << SPI_USR_MOSI_BITLEN_S) |
                 ((addr_bits - 1) << SPI_USR_ADDR_BITLEN_S);
  if (cmd_bits > 0)
    HSPI.user2 = ((cmd_bits - 1) << SPI_USR_COMMAND_BITLEN_S) | cmd;
  if (addr_bits > 0)
    HSPI.addr = addr << (32 - addr_bits);

  if (((uintptr_t)data) & 0x3 || len % 4 != 0) {
    const uint8_t *byte_buf = data;
    for (size_t i = 0; i < len;) {
      uint32_t d = byte_buf[i++];
      if (i < len)
        d |= ((uint32_t)byte_buf[i++]) << 8;
      if (i < len)
        d |= ((uint32_t)byte_buf[i++]) << 16;
      if (i < len)
        d |= ((uint32_t)byte_buf[i++]) << 24;
      HSPI.w[(i - 1) / 4] = d;
    }
  } else {
    const uint32_t *word_buf = data;
    for (size_t i = 0; i < len / 4; ++i)
      HSPI.w[i] = word_buf[i];
  }

  // start data transfer
  HSPI.cmd |= SPI_USR;

  return len;
}

static void apply_settings(struct hspi *settings, uint32_t user_reg) {
  static const uint32_t ctrl_mask = SPI_QIO_MODE | SPI_DIO_MODE;
  static const uint32_t user_mask = SPI_FWRITE_QIO | SPI_FWRITE_DIO;
  static const uint32_t cs_mask = SPI_CS0_DIS | SPI_CS1_DIS | SPI_CS2_DIS;

  switch (settings->mode) {
  case SPI_MODE_SPI:
    HSPI.ctrl &= ~ctrl_mask;
    HSPI.user = user_reg & ~user_mask;
    break;
  case SPI_MODE_DIO:
    HSPI.ctrl = (HSPI.ctrl & ~ctrl_mask) | SPI_DIO_MODE;
    HSPI.user = (user_reg & ~user_mask) | SPI_FWRITE_DIO;
    break;
  case SPI_MODE_QIO:
    HSPI.ctrl = (HSPI.ctrl & ~ctrl_mask) | SPI_QIO_MODE;
    HSPI.user = (user_reg & ~user_mask) | SPI_FWRITE_QIO;
    break;
  }

  switch (settings->cs) {
  case 0:
    HSPI.pin = (HSPI.pin & ~cs_mask) | SPI_CS1_DIS | SPI_CS2_DIS;
    break;
  case 1:
    HSPI.pin = (HSPI.pin & ~cs_mask) | SPI_CS0_DIS | SPI_CS2_DIS;
    break;
  case 2:
    HSPI.pin = (HSPI.pin & ~cs_mask) | SPI_CS0_DIS | SPI_CS1_DIS;
    break;
  }

  const uint32_t clkcnt_n = settings->clock_div - 1;
  const uint32_t clkcnt_h = settings->clock_div / 2 - 1;
  HSPI.clock = (clkcnt_n << SPI_CLKCNT_N_S) | (clkcnt_h << SPI_CLKCNT_H_S) |
               (clkcnt_n << SPI_CLKCNT_L_S);
}
