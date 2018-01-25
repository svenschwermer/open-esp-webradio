#ifndef _HSPI_H_
#define _HSPI_H_

#include "common_macros.h" // for IRAM
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum hspi_mode {
  SPI_MODE_SPI, // regular SPI
  SPI_MODE_DIO, // address & data on 2 lines simultaneously
  SPI_MODE_QIO, // address & data on 4 lines simultaneously
};

struct hspi {
  enum hspi_mode mode;
  int cs;
  unsigned int clock_div;
};

void hspi_init(void);
int hspi_init_inst(struct hspi *hspi);
size_t hspi_read(struct hspi *hspi, size_t len, void *data, int addr_bits,
                 uint32_t addr, int cmd_bits, uint16_t cmd,
                 int dummy_cycles) IRAM;
size_t hspi_write(struct hspi *hspi, size_t len, const void *data,
                  int addr_bits, uint32_t addr, int cmd_bits,
                  uint16_t cmd) IRAM;
uint32_t ads_read(int channel, bool single_ended) IRAM;

#endif
