#include "fifo.h"
#include "mi0283qt.h"
#include "mp3.h"
#include "stream_client.h"
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

static struct stream_params stream_params = {.host = "r.ezbt.me",
                                             .path = "/antenne"};
static TaskHandle_t mp3_task_hndl, stream_task_hndl;

void hexdump(const void *buf, size_t len) {
  size_t i = 0;
  const uint8_t *byte_buf = buf;
  while (i < len) {
    for (size_t j = 0; j < 16 && i < len; ++j, ++i)
      printf("%02x ", byte_buf[i]);
    printf("\n");
  }
}

void debug_timer(TimerHandle_t xTimer) {
  TickType_t period_ms = xTimerGetPeriod(xTimer) * portTICK_PERIOD_MS;
  printf("high water (words): mp3=%lu stream=%lu\n"
         "free heap: %u\n"
         "stream: %f bytes/s\n"
         "fifo: %u/%u\n"
         "underruns: %u\n\n",
         uxTaskGetStackHighWaterMark(mp3_task_hndl),
         uxTaskGetStackHighWaterMark(stream_task_hndl), xPortGetFreeHeapSize(),
         get_and_reset_streamed_bytes() * 1000.f / period_ms, fifo_fill(),
         fifo_size(), get_and_reset_underrun_counter());
}

void user_init(void) {
  int ret;

  uart_set_baud(0, 115200);

  if ((ret = fifo_init())) {
    printf("fifo_init failed (%d)\n", ret);
    goto fail;
  }

  if ((ret = wm8731_init())) {
    printf("wm8731_init failed (%d)\n", ret);
    goto fail;
  }

  if ((ret = lcd_init())) {
    printf("lcd_init failed (%d)\n", ret);
    goto fail;
  }

  struct sdk_station_config config = {
      .ssid = WIFI_SSID,
      .password = WIFI_PASS,
  };

  /* required to call wifi_set_opmode before station_set_config */
  sdk_wifi_set_opmode(STATION_MODE);
  sdk_wifi_station_set_config(&config);

  if (xTaskCreate(mp3_task, "consumer", 2100, NULL, 3, &mp3_task_hndl) !=
      pdPASS) {
    printf("Failed to create mp3 task!\n");
    goto fail;
  }

  if (xTaskCreate(stream_task, "producer", 384, &stream_params, 2,
                  &stream_task_hndl) != pdPASS) {
    printf("Failed to create stream task!\n");
    goto fail;
  }

  TimerHandle_t timer =
      xTimerCreate("debug", pdMS_TO_TICKS(3000), pdTRUE, NULL, debug_timer);
  if (timer != NULL)
    xTimerStart(timer, 0);
  else {
    printf("Failed to debug timer!\n");
    goto fail;
  }

  return;

fail:
  while (1)
    ;
}
