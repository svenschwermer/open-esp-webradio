#include "fifo.h"
#include "hspi.h"
#include "mi0283qt.h"
#include "mp3.h"
#include "stream_client.h"
#include "terminal.h"
#include "wm8731.h"

#include "esp/hwrand.h"
#include "esp/uart.h"
#include "esp8266.h"
#include "espressif/esp_common.h"
#include "ssid_config.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"

#include <stdio.h>
#include <string.h>

static struct stream_params stream_params = {.host = "r.ezbt.me",
                                             .path = "/antenne"};
static TaskHandle_t mp3_task_hndl, stream_task_hndl;

void ui_task(void *p) {
#if 1
  gpio_write(16, true);
  gpio_enable(16, GPIO_OUTPUT);
#endif

  for (int i = 0;; ++i) {
    uint32_t x = ads_read(5, false);
    uint32_t y = ads_read(1, false);
    uint32_t z1 = ads_read(3, false);
    uint32_t z2 = ads_read(4, false);
    uint32_t p;
    if (z1 > 0)
      p = ((z2 - z1) * x) / z1;
    else
      p = UINT32_MAX;

    printf("free heap: %u\nfifo: %u/%u\nunderruns: %u\n"
           "x=%u y=%u\nz1=%u z2=%u\npressure=%u\n\n",
           xPortGetFreeHeapSize(), fifo_fill(), fifo_size(),
           get_and_reset_underrun_counter(), x, y, z1, z2, p);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }

  vTaskDelete(NULL);
}

void user_init(void) {
  int ret;

  uart_set_baud(0, 115200);
  hspi_init();

  if ((ret = lcd_init())) {
    printf("lcd_init failed (%d)\n", ret);
    goto fail;
  }
  term_init();

  if ((ret = fifo_init())) {
    printf("fifo_init failed (%d)\n", ret);
    goto fail;
  }

  if ((ret = wm8731_init())) {
    printf("wm8731_init failed (%d)\n", ret);
    goto fail;
  }

  struct sdk_station_config config = {
      .ssid = WIFI_SSID,
      .password = WIFI_PASS,
  };

  /* required to call wifi_set_opmode before station_set_config */
  sdk_wifi_set_opmode(STATION_MODE);
  sdk_wifi_station_set_config(&config);

#if 0
  if (xTaskCreate(mp3_task, "consumer", 2100, NULL, 4, &mp3_task_hndl) !=
      pdPASS) {
    printf("Failed to create mp3 task!\n");
    goto fail;
  }

  if (xTaskCreate(stream_task, "producer", 384, &stream_params, 3,
                  &stream_task_hndl) != pdPASS) {
    printf("Failed to create stream task!\n");
    goto fail;
  }
#endif

  if (xTaskCreate(ui_task, "UI", 2 * configMINIMAL_STACK_SIZE, NULL, 1, NULL) !=
      pdPASS) {
    printf("Failed to create UI task!\n");
    goto fail;
  }

  return;

fail:
  while (1)
    ;
}
