#ifndef _SPIRAM_H_
#define _SPIRAM_H_

#include "common_macros.h" // for IRAM
#include <stdint.h>

#define SPIRAM_SIZE (128 * 1024) // 1 Mbit, 128 kB (23LC1024)

// Define this to use the SPI RAM in QSPI mode. This mode theoretically improves
// the bandwith to the chip four-fold, but it needs all 4 SDIO pins connected.
// It's  disabled here because not everyone using the MP3 example will have
// those pins  connected and the overall speed increase on the MP3 example is
// negligable.
//#define SPIRAM_QIO

// Define this if you have wired the SRAM:
// SDIO0 - SIO1
// SDIO1 - SIO0
// SDIO2 - SIO2
// SDIO3 - SIO3
#define SPIRAM_QIO_HACK

int spiram_init() IRAM;
size_t spiram_read(uint32_t addr, void *buf, size_t len) IRAM;
size_t spiram_write(uint32_t addr, const void *buf, size_t len) IRAM;
int spiram_test();

#endif
