#include "ads7846.h"

#include "hspi.h"
#include "mi0283qt.h"

#include "esp/gpio.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

// Calibration data
static uint32_t a_x = 1;
static uint32_t b_x = 0;
static uint32_t a_y = 1;
static uint32_t b_y = 0;

void ads_init(void) {
  gpio_write(16, true);
  gpio_enable(16, GPIO_OUTPUT);
}

bool ads_poll(uint32_t *x, uint32_t *y) {
  uint32_t z1 = ads_read(3, false);
  if (z1 > 200) {
    if (x) {
      *x = ads_read(5, false);
      *x = (*x - b_x) / a_x;
    }
    if (y) {
      *y = ads_read(1, false);
      *y = (*y - b_y) / a_y;
    }
    return true;
  }
  return false;
}

static inline void ads_draw_cross(uint16_t x, uint16_t y) {
  lcd_rect(RGB(63, 0, 0), x - 5, y, x + 5, y);
  lcd_rect(RGB(63, 0, 0), x, y - 5, x, y + 5);
}

void ads_calibrate(int bottom_lines) {
  const int margin = 20;
  enum {
    TOP_LEFT,
    TOP_RIGHT,
    BOTTOM_RIGHT,
    BOTTOM_LEFT,
    DONE
  } state = TOP_LEFT;
  uint32_t x[4];
  uint32_t y[4];
  uint16_t height = LCD_HEIGHT - bottom_lines;

  lcd_rect(RGB(63, 63, 63), 0, 0, LCD_WIDTH - 1, height - 1);
  ads_draw_cross(margin, margin);

  lcd_rect(RGB(0, 0, 0), 75, 27, 149, 51);
  lcd_string(80, 32, "TOUCHSCREEN");
  lcd_string(80, 40, "CALIBRATION");

  while (state != DONE) {
    vTaskDelay(2);
    if (ads_poll(x + state, y + state)) {
      switch (state) {
      case TOP_LEFT:
        ads_draw_cross(LCD_WIDTH - margin - 1, margin);
        state = TOP_RIGHT;
        break;
      case TOP_RIGHT:
        ads_draw_cross(LCD_WIDTH - margin - 1, height - margin - 1);
        state = BOTTOM_RIGHT;
        break;
      case BOTTOM_RIGHT:
        ads_draw_cross(margin, height - margin - 1);
        state = BOTTOM_LEFT;
        break;
      default:
        state = DONE;
        break;
      }
      if (state != DONE)
        vTaskDelay(10);
    }
  }

  a_x = (x[1] + x[2] - x[0] - x[3]) / 2 / (LCD_WIDTH - 2 * margin + 1);
  b_x = (x[0] + x[3]) / 2 - (margin * a_x);
  a_y = (y[2] + y[3] - y[0] - y[1]) / 2 / (height - 2 * margin + 1);
  b_y = (y[0] + y[1]) / 2 - (margin * a_y);

  lcd_rect(RGB(63, 63, 63), 0, 0, LCD_WIDTH - 1, height - 1);

  printf("a_x=%u b_x=%u\n", a_x, b_x);
  printf("a_y=%u b_y=%u\n", a_y, b_y);
}
