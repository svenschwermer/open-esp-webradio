#include "ads7846.h"
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

void ui_task(void *p) {
  ads_init();

  extern const struct image img_arrow_left;
  lcd_image(0, 0, &img_arrow_left);
  extern const struct image img_arrow_right;
  lcd_image(LCD_WIDTH - img_arrow_right.width, 0, &img_arrow_right);
  extern const struct image img_standby;
  lcd_image(0, 192, &img_standby);
  extern const struct image img_settings;
  lcd_image(48, 192, &img_settings);
  extern const struct image img_play;
  lcd_image(96, 192, &img_play);
  extern const struct image img_vol_minus;
  lcd_image(144, 192, &img_vol_minus);
  extern const struct image img_vol_plus;
  lcd_image(192, 192, &img_vol_plus);

  for (int i = 0;; ++i) {
    vTaskDelay(2);

    if (ads_poll(NULL, NULL)) {

      // lcd_scroll_off();
      // ads_calibrate();
      // lcd_scroll_on(0, 0);
    }

    unsigned int underruns = get_and_reset_underrun_counter();
    if (underruns)
      printf("!!! %u underruns\n", underruns);

#if 0
    printf("free heap: %u\nfifo: %u/%u\nunderruns: %u\n\n",
           xPortGetFreeHeapSize(), fifo_fill(), fifo_size(),
           get_and_reset_underrun_counter());
#endif
  }

  vTaskDelete(NULL);
}

static void stream_up(void) {
  if (xTaskCreate(mp3_task, "decode", 2100, NULL, 4, NULL) != pdPASS) {
    printf("Failed to create mp3 task!\n");
  }
}

static void stream_metadata(enum stream_metadata type, const char *s) {
  switch (type) {
  case STREAM_ARTIST:
    printf("Artist: %s\n", s);
    break;
  case STREAM_TITLE:
    printf("Title: %s\n", s);
    break;
  }
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

  if (stream_start("r.ezbt.me", "/antenne", stream_up, stream_metadata)) {
    printf("Failed to create stream task!\n");
    goto fail;
  }

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
