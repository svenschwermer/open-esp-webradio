#include "ads7846.h"

#include "hspi.h"
#include "mi0283qt.h"

#include "esp/gpio.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

void ads_init(void) {
  gpio_write(16, true);
  gpio_enable(16, GPIO_OUTPUT);
}

bool ads_poll(uint32_t *x, uint32_t *y) {
  uint32_t z1 = ads_read(3, false);
  if (z1 > 200) {
    if (x)
      *x = ads_read(5, false);
    if (y)
      *y = ads_read(1, false);
    return true;
  }
  return false;
}

static inline void ads_draw_cross(uint16_t x, uint16_t y) {
  lcd_rect(RGB(63, 0, 0), x - 5, y, x + 5, y);
  lcd_rect(RGB(63, 0, 0), x, y - 5, x, y + 5);
}

void ads_calibrate(void) {
  enum {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_RIGHT,
    BOTTOM_LEFT,
    DONE
  } state = TOP_LEFT;
  uint32_t x[4];
  uint32_t y[4];

  lcd_fill(RGB(63, 63, 63));
  ads_draw_cross(19, 19);

  while (state != DONE) {
    vTaskDelay(5);
    if (ads_poll(x + state, y + state)) {
      switch (state) {
      case TOP_LEFT:
        ads_draw_cross(LCD_WIDTH - 20, 19);
        state = TOP_RIGHT;
        break;
      case TOP_RIGHT:
        ads_draw_cross(LCD_WIDTH - 20, LCD_HEIGHT - 20);
        state = BOTTOM_RIGHT;
        break;
      case BOTTOM_RIGHT:
        ads_draw_cross(19, LCD_HEIGHT - 20);
        state = BOTTOM_LEFT;
        break;
      default:
        state = DONE;
        break;
      }
    }
  }

  uint32_t a_x = (x[1] + x[2] - x[0] - x[3]) / (2 * 201);
  uint32_t b_x = (x[0] + x[3]) / 2 - (19 * a_x);
  uint32_t a_y = (y[2] + y[3] - y[0] - y[1]) / (2 * 281);
  uint32_t b_y = (y[0] + y[1]) / 2 - (19 * a_y);

  printf("a_x=%u b_x=%u\n", a_x, b_x);
  printf("a_y=%u b_y=%u\n", a_y, b_y);

  while (true) {
    vTaskDelay(5);
    if (ads_poll(x, y)) {
      *x = (*x - b_x) / a_x;
      *y = (*y - b_y) / a_y;
      printf("touch (%u,%u)\n", *x, *y);
    }
  }
}
