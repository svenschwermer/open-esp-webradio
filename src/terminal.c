#include "lcd_font.h"
#include "mi0283qt.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include <unistd.h>

#include <stdio.h>
#include <stdout_redirect.h>

#define CHARS_PER_LINE (LCD_WIDTH / (FONT_WIDTH + FONT_MARGIN))

static uint16_t y;
static SemaphoreHandle_t mtx;

static ssize_t term_stdout(struct _reent *r, int fd, const void *ptr,
                           size_t len);

void term_init(void) {
  y = 0;
  mtx = xSemaphoreCreateMutex();

  lcd_fill(RGB(0, 0, 0));
  lcd_scroll_on(0, 0);
  set_write_stdout(term_stdout);
  printf("Terminal ok\n");
}

static ssize_t term_stdout(struct _reent *r, int fd, const void *ptr,
                           size_t len) {
  const char *str = (const char *)ptr;
  size_t i = 0;

  xSemaphoreTake(mtx, portMAX_DELAY);

  while (i < len) {
    size_t j;
    for (j = 0; (i + j) < len && j < CHARS_PER_LINE; ++j) {
      if (str[i + j] == '\n')
        break;
    }
    int x = lcd_stringn(0, y, str + i, j);
    if (x < LCD_WIDTH) {
      lcd_rect(RGB(0, 0, 0), x, y, LCD_WIDTH - 1,
               y + FONT_HEIGHT + FONT_MARGIN - 1);
    }

    y = (y + FONT_HEIGHT + FONT_MARGIN) % LCD_HEIGHT;
    lcd_scroll(y);
    i += j;

    if (str[i] == '\n')
      ++i;
  }

  xSemaphoreGive(mtx);

  return i;
}
