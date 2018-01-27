#include "mi0283qt.h"
#include "FreeRTOS.h"
#include "common.h"
#include "hspi.h"
#include "lcd_font.h"
#include "task.h"
#include <string.h>

#define LCD_ID 0
#define LCD_DATA ((0x72) | (LCD_ID << 2))
#define LCD_REGISTER ((0x70) | (LCD_ID << 2))

static struct hspi hspi;
static uint16_t pixel_buffer[32];

struct init_step {
  enum {
    STEP_TYPE_CMD,
    STEP_TYPE_DELAY,
  } type;
  union {
    struct {
      uint8_t cmd;
      uint8_t data;
    } cmd;
    uint16_t delay_ms;
  };
};

static const struct init_step init_steps[] = {
    // driving ability
    {STEP_TYPE_CMD, .cmd = {0xEA, 0x00}}, // Power control internal use (1)
    {STEP_TYPE_CMD, .cmd = {0xEB, 0x20}}, // Power control internal use (2)
    {STEP_TYPE_CMD, .cmd = {0xEC, 0x0C}}, // Source control internal use (1)
    {STEP_TYPE_CMD, .cmd = {0xED, 0xC4}}, // Source control internal use (2)
    {STEP_TYPE_CMD, .cmd = {0xE8, 0x40}}, // Source OP control_Normal
    {STEP_TYPE_CMD, .cmd = {0xE9, 0x38}}, // Source OP control_IDLE
    {STEP_TYPE_CMD, .cmd = {0xF1, 0x01}},
    {STEP_TYPE_CMD, .cmd = {0xF2, 0x10}},
    {STEP_TYPE_CMD, .cmd = {0x27, 0xA3}}, // Display Control 2
    // power voltage
    {STEP_TYPE_CMD, .cmd = {0x1B, 0x1B}}, // Power Control 2
    {STEP_TYPE_CMD, .cmd = {0x1A, 0x01}}, // Power Control 1
    {STEP_TYPE_CMD, .cmd = {0x24, 0x2F}}, // VCOM Control 2
    {STEP_TYPE_CMD, .cmd = {0x25, 0x57}}, // VCOM Control 3
    // VCOM offset
    {STEP_TYPE_CMD, .cmd = {0x23, 0x8D}}, // VCOM Control 1
    // power on
    {STEP_TYPE_CMD, .cmd = {0x18, 0x36}}, // OSC Control 2
    {STEP_TYPE_CMD, .cmd = {0x19, 0x01}}, // OSC Control 1: start osc
    {STEP_TYPE_CMD, .cmd = {0x01, 0x00}}, // Display Mode control: wakeup
    {STEP_TYPE_CMD, .cmd = {0x1F, 0x88}}, // Power Control 6
    {STEP_TYPE_DELAY, .delay_ms = 5},
    {STEP_TYPE_CMD, .cmd = {0x1F, 0x80}},
    {STEP_TYPE_DELAY, .delay_ms = 5},
    {STEP_TYPE_CMD, .cmd = {0x1F, 0x90}},
    {STEP_TYPE_DELAY, .delay_ms = 5},
    {STEP_TYPE_CMD, .cmd = {0x1F, 0xD0}},
    {STEP_TYPE_DELAY, .delay_ms = 5},
    // color selection: 0x05=65k, 0x06=262k
    {STEP_TYPE_CMD, .cmd = {0x17, 0x05}},
    // panel characteristic
    {STEP_TYPE_CMD, .cmd = {0x36, 0x00}},
    // Memory Access control
    {STEP_TYPE_CMD, .cmd = {0x16, 0x08}}, // MY=0 MX=0 MV=0 ML=0 BGR=1
    // display on
    {STEP_TYPE_CMD, .cmd = {0x28, 0x38}},
    {STEP_TYPE_DELAY, .delay_ms = 50},
    {STEP_TYPE_CMD, .cmd = {0x28, 0x3C}},
    {STEP_TYPE_DELAY, .delay_ms = 5}};

static inline void wr_cmd(uint8_t cmd, uint8_t data) {
  hspi_write(&hspi, 1, &cmd, 0, 0, 8, LCD_REGISTER);
  hspi_write(&hspi, 1, &data, 0, 0, 8, LCD_DATA);
}

static inline void wr_sram(void) {
  const uint8_t cmd = 0x22; // SRAM Write Control
  hspi_write(&hspi, 1, &cmd, 0, 0, 8, LCD_REGISTER);
}

static inline void wr_pixels(size_t count, const uint16_t *pixels) {
  size_t rem_bytes = count * sizeof(pixels[0]);
  while (rem_bytes > 0)
    rem_bytes -= hspi_write(&hspi, rem_bytes, pixels, 0, 0, 8, LCD_DATA);
}

int lcd_init() {
  hspi.mode = SPI_MODE_SPI;
  hspi.cs = 1;
  hspi.clock_div = 2; // 40 MHz
  if (hspi_init(&hspi))
    return 1;

  TickType_t ticks;
  for (int i = 0; i < ARRAY_SIZE(init_steps); ++i) {
    const struct init_step step = init_steps[i];
    switch (step.type) {
    case STEP_TYPE_CMD:
      wr_cmd(step.cmd.cmd, step.cmd.data);
      break;

    case STEP_TYPE_DELAY:
      ticks = step.delay_ms / portTICK_PERIOD_MS;
      if (ticks == 0)
        ticks = 1;
      vTaskDelay(ticks);
      break;
    }
  }

  lcd_fill(RGB(63, 63, 63));

  return 0;
}

// sets the drawing area to [x0,x1] x [y0,y1]
// note that x1 and y1 are included
void lcd_set_area(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
  wr_cmd(0x03, (uint8_t)(x0 & 0xff)); // set x0
  wr_cmd(0x02, (uint8_t)(x0 >> 8));   // set x0
  wr_cmd(0x05, (uint8_t)(x1 & 0xff)); // set x1
  wr_cmd(0x04, (uint8_t)(x1 >> 8));   // set x1
  wr_cmd(0x07, (uint8_t)(y0 & 0xff)); // set y0
  wr_cmd(0x06, (uint8_t)(y0 >> 8));   // set y0
  wr_cmd(0x09, (uint8_t)(y1 & 0xff)); // set y1
  wr_cmd(0x08, (uint8_t)(y1 >> 8));   // set y1
}

void lcd_write_pixels(size_t count, const uint16_t *pixels) {
  wr_sram();
  wr_pixels(count, pixels);
}

void lcd_rect(uint16_t color, uint16_t x0, uint16_t y0, uint16_t x1,
              uint16_t y1) {
  lcd_set_area(x0, y0, x1, y1);

  for (int i = 0; i < ARRAY_SIZE(pixel_buffer); ++i)
    pixel_buffer[i] = color;

  wr_sram();

  size_t rem_bytes = (x1 - x0 + 1) * (y1 - y0 + 1) * sizeof(pixel_buffer[0]);
  while (rem_bytes > 0)
    rem_bytes -= hspi_write(&hspi, rem_bytes, pixel_buffer, 0, 0, 8, LCD_DATA);
}

void lcd_fill(uint16_t color) {
  lcd_rect(color, 0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
}

void lcd_xy_exchange(bool exchange) {
  // Memory Access control
  if (exchange)
    wr_cmd(0x16, 0x28);
  else
    wr_cmd(0x16, 0x08); // default: MV=0
}

int lcd_string(int x, int y, const char *str) {
  return lcd_stringn(x, y, str, strlen(str));
}

int lcd_stringn(int x, int y, const char *str, size_t n) {
  lcd_xy_exchange(true);
  for (size_t i = 0; i < n; ++i, ++str) {
    // jump over non-printable characters
    if (*str < FIRST_CHAR || *str >= FIRST_CHAR + CHAR_COUNT)
      continue;

    // x & y need to be flipped, because we enable xy exchange
    lcd_set_area(y, x, y + FONT_HEIGHT - 1, x + FONT_WIDTH + FONT_MARGIN - 1);
    wr_sram();

    int buf_pos = 0;
    for (int col = 0; col < FONT_WIDTH; ++col) {
      uint8_t col_byte = font[(*str - FIRST_CHAR) * FONT_WIDTH + col];
      for (int line = 0; line < FONT_HEIGHT; ++line) {
        if (col_byte & (1 << line))
          pixel_buffer[buf_pos] = RGB(63, 63, 63);
        else
          pixel_buffer[buf_pos] = RGB(0, 0, 0);
        ++buf_pos;
      }

      if (buf_pos + FONT_HEIGHT > ARRAY_SIZE(pixel_buffer)) {
        wr_pixels(buf_pos, pixel_buffer);
        buf_pos = 0;
      }
    }

    // draw horizontal margin
    for (int line = FONT_HEIGHT - 1; line >= 0; --line)
      pixel_buffer[buf_pos++] = RGB(0, 0, 0);

    // write remaining pixel data
    wr_pixels(buf_pos, pixel_buffer);

    x += (FONT_WIDTH + FONT_MARGIN);
  }
  lcd_xy_exchange(false);
  return x;
}

void lcd_scroll_on(uint16_t top_fixed, uint16_t bottom_fixed) {
  // Vertical scroll top fixed area register
  wr_cmd(0x0e, top_fixed >> 8);
  wr_cmd(0x0f, top_fixed & 0xff);

  // Vertical scroll height area register
  uint16_t scroll_height = LCD_HEIGHT - top_fixed - bottom_fixed;
  wr_cmd(0x10, scroll_height >> 8);
  wr_cmd(0x11, scroll_height & 0xff);

  // Vertical scroll button fixed area register
  wr_cmd(0x12, bottom_fixed >> 8);
  wr_cmd(0x13, bottom_fixed & 0xff);

  wr_cmd(0x01, 0x08); // SCROLL=1
}

void lcd_scroll(uint16_t lines) {
  // Vertical scroll start address register
  wr_cmd(0x14, lines >> 8);
  wr_cmd(0x15, lines & 0xff);
}
