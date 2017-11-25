#include "spiram.h"
#include "espressif/esp8266/esp8266.h"
#include "espressif/esp_common.h"

#include "FreeRTOS.h"
#include "task.h"

struct spi_regs {
  uint32_t cmd;       // 0x00
  uint32_t addr;      // 0x04
  uint32_t ctrl;      // 0x08
  uint32_t _1;        // 0x0c
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
  uint32_t _2[31];    // 0x80..0xf8
  uint32_t ext3;      // 0xfc
};

static volatile struct spi_regs *spi = (struct spi_regs *)(REG_SPI_BASE(0));
static volatile struct spi_regs *hspi = (struct spi_regs *)(REG_SPI_BASE(1));

uint8_t IRAM spi_read(uint8_t cmd) {

  while (hspi->cmd & SPI_USR)
    ;

  hspi->user |= SPI_CS_SETUP | SPI_CS_HOLD | SPI_USR_COMMAND | SPI_USR_MISO;
  hspi->user &= ~(SPI_FLASH_MODE | SPI_USR_MOSI | SPI_USR_ADDR | SPI_USR_DUMMY);
  hspi->user1 = 7 << SPI_USR_MISO_BITLEN_S;
  hspi->user2 = (7 << SPI_USR_COMMAND_BITLEN_S) | cmd;

  hspi->cmd |= SPI_USR;
  while (hspi->cmd & SPI_USR)
    ;

  return (uint8_t)(hspi->w[0] & 0xff);
}

void IRAM spi_write(uint8_t cmd, uint8_t data) {
  while (hspi->cmd & SPI_USR)
    ;

  hspi->user |= SPI_CS_SETUP | SPI_CS_HOLD | SPI_USR_COMMAND | SPI_USR_MOSI;
  hspi->user &= ~(SPI_FLASH_MODE | SPI_USR_MISO | SPI_USR_ADDR | SPI_USR_DUMMY);
  hspi->user1 = 7 << SPI_USR_MOSI_BITLEN_S;
  hspi->user2 = (7 << SPI_USR_COMMAND_BITLEN_S) | cmd;
  hspi->w[0] = data;

  hspi->cmd |= SPI_USR;
  while (hspi->cmd & SPI_USR)
    ;
}

void IRAM spi_cmd(uint8_t cmd) {
  while (hspi->cmd & SPI_USR)
    ;

  hspi->user |= SPI_CS_SETUP | SPI_CS_HOLD | SPI_USR_COMMAND;
  hspi->user &= ~(SPI_FLASH_MODE | SPI_USR_MISO | SPI_USR_MOSI | SPI_USR_ADDR |
                  SPI_USR_DUMMY);
  hspi->user2 = (7 << SPI_USR_COMMAND_BITLEN_S) | cmd;

  hspi->cmd |= SPI_USR;
  while (hspi->cmd & SPI_USR)
    ;
}

// Initialize the SPI_NUM port to talk to the chip.
void spiram_init() {

  printf("HSPI_USER=0x%08x\n", hspi->user);

  taskENTER_CRITICAL();

  // hspi overlap to spi, two spi masters on cspi
  SET_PERI_REG_MASK(HOST_INF_SEL, PERI_IO_CSPI_OVERLAP);

  // set higher priority for spi than hspi
  spi->ext3 |= 0x1;
  hspi->ext3 |= 0x3;

  hspi->user |= SPI_CS_SETUP;

  // select HSPI_NUM CS2 ,disable HSPI_NUM CS0 and CS1
  hspi->pin &= ~SPI_CS2_DIS;
  hspi->pin |= SPI_CS0_DIS | SPI_CS1_DIS;

  // SET IO MUX FOR GPIO0 , SELECT PIN FUNC AS SPI_NUM CS2
  // IT WORK AS HSPI_NUM CS2 AFTER OVERLAP(THERE IS NO PIN OUT FOR NATIVE
  // HSPI_NUM CS1/2)
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_SPICS2);

  // SPI_NUM CLK = 80 MHz / 4 = 20 MHz
  hspi->clock =
      (3 << SPI_CLKCNT_N_S) | (1 << SPI_CLKCNT_H_S) | (3 << SPI_CLKCNT_L_S);

#ifdef SPIRAM_QIO
  hspi->user |= SPI_CS_SETUP | SPI_CS_HOLD | SPI_USR_COMMAND;
  hspi->user &= ~SPI_FLASH_MODE;

  spi_write(0x01, 0x40);                   // set mode: sequential mode
  printf("mode=0x%02x\n", spi_read(0x05)); // read back mode
  spi_cmd(0x38);                           // enter quad I/O mode

  hspi->ctrl |= SPI_QIO_MODE | SPI_FASTRD_MODE;
  hspi->user |= SPI_FWRITE_QIO;
#endif
  taskEXIT_CRITICAL();
}

// Read bytes from a memory location. The max amount of bytes that can be read
// is 64.
size_t spiram_read(uint32_t addr, void *buf, size_t len) {
  uint8_t *byte_buf = buf;
  if (len > 64)
    len = 64;

  taskENTER_CRITICAL();

  while (hspi->cmd & SPI_USR)
    ;

#ifndef SPIRAM_QIO
  hspi->user |= SPI_CS_SETUP | SPI_CS_HOLD | SPI_USR_COMMAND | SPI_USR_ADDR |
                SPI_USR_MISO;
  hspi->user &= ~(SPI_FLASH_MODE | SPI_USR_MOSI);
  hspi->user1 = ((((8 * len) - 1) & SPI_USR_MISO_BITLEN)
                 << SPI_USR_MISO_BITLEN_S) | // len bits of data in
                ((23 & SPI_USR_ADDR_BITLEN)
                 << SPI_USR_ADDR_BITLEN_S); // address is 24 bits A0-A23
  hspi->addr = addr << 8;                   // write address
  hspi->user2 =
      ((7 & SPI_USR_COMMAND_BITLEN) << SPI_USR_COMMAND_BITLEN_S) | 0x03;
#else
  hspi->user |= SPI_USR_ADDR | SPI_USR_MISO | SPI_USR_DUMMY;
  hspi->user &= ~(SPI_FLASH_MODE | SPI_USR_MOSI | SPI_USR_COMMAND);
  hspi->user1 = ((((8 * len) - 1) & SPI_USR_MISO_BITLEN)
                 << SPI_USR_MISO_BITLEN_S) | // len bits of data in
                ((31 & SPI_USR_ADDR_BITLEN)
                 << SPI_USR_ADDR_BITLEN_S) | // 8bits command+address is 24 bits
                                             // A0-A23
                ((1 & SPI_USR_DUMMY_CYCLELEN)
                 << SPI_USR_DUMMY_CYCLELEN_S); // 8bits dummy cycle

  addr |= 0x03000000; // prepend command

#ifdef SPIRAM_QIO_HACK
  addr = (addr & 0x33333333) | ((addr & 0x88888888) >> 1) |
         ((addr & 0x44444444) << 1); // swap SIO2 and SIO3
#endif

  hspi->addr = addr; // command & write address
#endif

  hspi->cmd |= SPI_USR;
  while (hspi->cmd & SPI_USR)
    ;

  // Unaligned dest address. Copy 8bit at a time
  for (size_t i = 0; i < len;) {
    uint32_t d = hspi->w[i / 4];
    byte_buf[i++] = d & 0xff;
    if (i < len)
      byte_buf[i++] = (d >> 8) & 0xff;
    if (i < len)
      byte_buf[i++] = (d >> 16) & 0xff;
    if (i < len)
      byte_buf[i++] = (d >> 24) & 0xff;
  }

  taskEXIT_CRITICAL();

  return len;
}

// Write bytes to a memory location. The max amount of bytes that can be written
// is 64.
size_t spiram_write(uint32_t addr, const void *buf, size_t len) {
  const uint8_t *byte_buf = buf;
  if (len > 64)
    len = 64;

  taskENTER_CRITICAL();

  while (hspi->cmd & SPI_USR)
    ;

#ifndef SPIRAM_QIO
  hspi->user |= SPI_CS_SETUP | SPI_CS_HOLD | SPI_USR_COMMAND | SPI_USR_ADDR |
                SPI_USR_MOSI;
  hspi->user &= ~(SPI_FLASH_MODE | SPI_USR_MISO);
  hspi->user1 = ((((8 * len) - 1) & SPI_USR_MOSI_BITLEN)
                 << SPI_USR_MOSI_BITLEN_S) | // len bitsbits of data out
                ((23 & SPI_USR_ADDR_BITLEN)
                 << SPI_USR_ADDR_BITLEN_S); // address is 24 bits A0-A23
  hspi->addr = addr << 8;                   // write address
  hspi->user2 =
      (((7 & SPI_USR_COMMAND_BITLEN) << SPI_USR_COMMAND_BITLEN_S) | 0x02);
#else
  // No command phase; the command is the top-most byte of the address
  // Address phase is 32 bits long
  // No dummy phase in the write operation
  // No read-data phase, but a write-data phase

  hspi->user |= SPI_USR_ADDR | SPI_USR_MOSI;
  hspi->user &=
      ~(SPI_FLASH_MODE | SPI_USR_MISO | SPI_USR_COMMAND | SPI_USR_DUMMY);
  hspi->user1 = ((((8 * len) - 1) & SPI_USR_MOSI_BITLEN)
                 << SPI_USR_MOSI_BITLEN_S) | // len bitsbits of data out
                ((31 & SPI_USR_ADDR_BITLEN)
                 << SPI_USR_ADDR_BITLEN_S); // 8bits command+address is 24 bits
                                            // A0-A23

  addr |= 0x02000000; // prepend command

#ifdef SPIRAM_QIO_HACK
  addr = (addr & 0x33333333) | ((addr & 0x88888888) >> 1) |
         ((addr & 0x44444444) << 1); // swap SIO2 and SIO3
#endif

  hspi->addr = addr; // write command & address
#endif

  // Assume unaligned src: Copy byte-wise.
  for (size_t i = 0; i < len;) {
    uint32_t d = byte_buf[i++];
    if (i < len)
      d |= ((uint32_t)byte_buf[i++]) << 8;
    if (i < len)
      d |= ((uint32_t)byte_buf[i++]) << 16;
    if (i < len)
      d |= ((uint32_t)byte_buf[i++]) << 24;
    hspi->w[(i - 1) / 4] = d;
  }
  hspi->cmd |= SPI_USR;

  taskEXIT_CRITICAL();

  return len;
}

// Simple routine to see if the SPI_NUM actually stores bytes. This is not a
// full memory test, but will tell  you if the RAM chip is connected well.
int spiram_test() {
  int x;
  int err = 0;
  char a[64];
  char b[64];
  char aa, bb;
  for (x = 0; x < 64; x++) {
    a[x] = x ^ (x << 2);
    b[x] = 0xaa ^ x;
  }
  spiram_write(0x0, a, 64);
  spiram_write(0x100, b, 64);

  spiram_read(0x0, a, 64);
  spiram_read(0x100, b, 64);
  for (x = 0; x < 64; x++) {
    aa = x ^ (x << 2);
    bb = 0xaa ^ x;
    if (aa != a[x]) {
      err = 1;
      printf("%i) aa: 0x%x != 0x%x\n", x, aa, a[x]);
    }
    if (bb != b[x]) {
      err = 1;
      printf("%i) bb: 0x%x != 0x%x\n", x, bb, b[x]);
    }
  }

  char buf[2] = {0x55, 0xaa};
  spiram_write(0x1, buf, 1);
  spiram_write(0x2, buf, 2);
  spiram_read(0x1, buf + 1, 1);
  if (buf[0] != buf[1]) {
    err = 1;
    printf("0x%x != 0x%x\n", buf[0], buf[1]);
  }

  return err;
}
