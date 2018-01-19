#include "spiram.h"
#include "espressif/esp8266/esp8266.h"
#include "espressif/esp_common.h"
#include "hspi.h"

#include "FreeRTOS.h"
#include "task.h"

static struct hspi hspi;

int spiram_init() {
  taskENTER_CRITICAL();

  hspi.cs = 2;
  hspi.mode = SPI_MODE_SPI;
  hspi.clock_div = 4; // 20 MHz
  if (hspi_init(&hspi))
    return 1;

  uint8_t mode = 0x00;
  for (int try = 1; try <= 2 && mode != 0x40; ++try) {
    if (try == 2) {
      // maybe the SPIRAM is in QIO mode, try RSTIO command
      hspi.mode = SPI_MODE_QIO;
      uint8_t rstio = 0xff;
      hspi_write(&hspi, 1, &rstio, 0, 0, 0, 0);
      hspi.mode = SPI_MODE_SPI;
    }

    // set mode: sequential mode
    mode = 0x40;
    hspi_write(&hspi, 1, &mode, 0, 0, 8, 0x01);

    // read back mode register
    hspi_read(&hspi, 1, &mode, 0, 0, 8, 0x05, 0);
    printf("spiram: read-back mode = 0x%02x\n", mode);
  }
  if (mode != 0x40)
    return 1;

#ifdef SPIRAM_QIO
  // enter quad I/O mode
  hspi_write(&hspi, 0, NULL, 0, 0, 8, 0x38);
  hspi.mode = SPI_MODE_QIO;
#endif

  taskEXIT_CRITICAL();

  uint8_t dummy[64];
  spiram_read(0x000000, dummy, sizeof dummy);

  return 0;
}

size_t spiram_read(uint32_t addr, void *buf, size_t len) {
  size_t read;

  taskENTER_CRITICAL();

#ifdef SPIRAM_QIO
  addr |= 0x03000000; // prepend command

#ifdef SPIRAM_QIO_HACK
  addr = (addr & 0x33333333) | ((addr & 0x88888888) >> 1) |
         ((addr & 0x44444444) << 1); // swap SIO2 and SIO3
#endif

  read = hspi_read(&hspi, len, buf, 32, addr, 0, 0, 2);
#else
  read = hspi_read(&hspi, len, buf, 24, addr, 8, 0x03, 0);
#endif

  taskEXIT_CRITICAL();

  return read;
}

size_t spiram_write(uint32_t addr, const void *buf, size_t len) {
  size_t written;

  taskENTER_CRITICAL();

#ifdef SPIRAM_QIO
  addr |= 0x02000000; // prepend command

#ifdef SPIRAM_QIO_HACK
  addr = (addr & 0x33333333) | ((addr & 0x88888888) >> 1) |
         ((addr & 0x44444444) << 1); // swap SIO2 and SIO3
#endif

  written = hspi_write(&hspi, len, buf, 32, addr, 0, 0);
#else
  written = hspi_write(&hspi, len, buf, 24, addr, 8, 0x02);
#endif

  taskEXIT_CRITICAL();

  return written;
}

// Simple routine to see if the SPI_NUM actually stores bytes. This is not a
// full memory test, but will tell  you if the RAM chip is connected well.
int spiram_test() {
  int err = 0;
  const int len = 64;
  char a[len];
  char b[len];
  char aa, bb;

  for (int x = 0; x < len; x++) {
    a[x] = x + 1;
    b[x] = len - x;
  }
  spiram_write(0x0, a, len);
  spiram_write(0x100, b, len);

  spiram_read(0x0, a, len);
  spiram_read(0x100, b, len);
  for (int x = 0; x < len; x++) {
    if (a[x] != x + 1 || b[x] != len - x) {
      err = 1;
      printf("a[%d]=%d b[%d]=%d\n", x, a[x], x, b[x]);
    }
  }

  for (int x = 0; x < len; x++) {
    a[x] = x ^ (x << 2);
    b[x] = 0xaa ^ x;
  }
  spiram_write(0x0, a, len);
  spiram_write(0x100, b, len);

  spiram_read(0x0, a, len);
  spiram_read(0x100, b, len);
  for (int x = 0; x < len; x++) {
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
