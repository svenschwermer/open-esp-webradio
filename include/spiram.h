#ifndef _SPIRAM_H_
#define _SPIRAM_H_

#include "common_macros.h" // for IRAM

#define SPIRAMSIZE (128*1024) //for a 23LC1024 chip

//Define this to use the SPI RAM in QSPI mode. This mode theoretically improves
//the bandwith to the chip four-fold, but it needs all 4 SDIO pins connected. It's
//disabled here because not everyone using the MP3 example will have those pins 
//connected and the overall speed increase on the MP3 example is negligable.
#define SPIRAM_QIO

// Define this if you have wired the SRAM SDIO0-SIO0, SDIO1-SIO1, SDIO2-SIO2, SDIO3-SIO3
#define SPIRAM_QIO_HACK

void spiRamInit() IRAM;
int spiRamRead(int addr, char *buff, int len) IRAM;
int spiRamWrite(int addr, const char *buff, int len) IRAM;
int spiRamTest();

#endif
