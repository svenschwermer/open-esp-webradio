#include "lcd_font.h"
#include "mi0283qt.h"

#include "FreeRTOS.h"
#include "semphr.h"

#include <unistd.h>

#include <stdio.h>
#include <stdout_redirect.h>

#define COL_WIDTH (FONT_WIDTH + FONT_MARGIN)
#define LINE_HEIGHT (FONT_HEIGHT + FONT_MARGIN)
#define Y_START 0 // (LCD_HEIGHT * 3 / 4)

static uint16_t x, y;

static ssize_t term_stdout(struct _reent *r, int fd, const void *ptr,
                           size_t len);

void term_init(void) {
  x = 0;
  y = Y_START;

  lcd_rect(RGB(0, 0, 0), 0, Y_START, LCD_WIDTH - 1, LCD_HEIGHT - 1);
  lcd_scroll_on(Y_START, 0);
  set_write_stdout(term_stdout);
  printf("Terminal ok\n");
}

static ssize_t term_stdout(struct _reent *r, int fd, const void *ptr,
                           size_t len) {
  const char *str = (const char *)ptr;

  size_t i = 0;
  while (i < len) {
    bool line_feed = false;
    size_t j = 0;
    while (i + j < len && !line_feed) {
      if (x + j * COL_WIDTH >= LCD_WIDTH || str[i + j] == '\n')
        line_feed = true;
      else
        ++j;
    }

    if (j > 0)
      x = lcd_stringn(x, y, str + i, j);
    if (x < LCD_WIDTH)
      lcd_rect(RGB(0, 0, 0), x, y, LCD_WIDTH - 1, y + LINE_HEIGHT - 1);

    if (line_feed) {
      x = 0;
      y = (y + LINE_HEIGHT);
      if (y >= LCD_HEIGHT)
        y = Y_START;
      lcd_scroll(y);
    }

    i += j;

    if (str[i] == '\n')
      ++i;
  }

  return i;
}
