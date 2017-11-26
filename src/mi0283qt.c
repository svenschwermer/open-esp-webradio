#include "mi0283qt.h"
#include "hspi.h"

#include "FreeRTOS.h"
#include "task.h"

#define ARRAY_SIZE(x) ((sizeof(x)) / (sizeof((x)[0])))

#define LCD_ID 0
#define LCD_DATA ((0x72) | (LCD_ID << 2))
#define LCD_REGISTER ((0x70) | (LCD_ID << 2))

#define LCD_WIDTH 320
#define LCD_HEIGHT 240

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
    // display options
    {STEP_TYPE_CMD, .cmd = {0x16, 0xA8}}, // Memory Access control
    // display on
    {STEP_TYPE_CMD, .cmd = {0x28, 0x38}},
    {STEP_TYPE_DELAY, .delay_ms = 50},
    {STEP_TYPE_CMD, .cmd = {0x28, 0x3C}},
    {STEP_TYPE_DELAY, .delay_ms = 5}};

static inline void wr_cmd(uint8_t cmd, uint8_t data) {
  hspi_write(&hspi, 1, &cmd, 0, 0, 8, LCD_REGISTER);
  hspi_write(&hspi, 1, &data, 0, 0, 8, LCD_DATA);
}

int lcd_init() {
  hspi.mode = SPI_MODE_SPI;
  hspi.cs = 1;
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

void lcd_fill(uint16_t color) {
  lcd_set_area(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

  for (int i = 0; i < ARRAY_SIZE(pixel_buffer); ++i)
    pixel_buffer[i] = color;

  const uint8_t cmd = 0x22; // SRAM Write Control
  hspi_write(&hspi, 1, &cmd, 0, 0, 8, LCD_REGISTER);

  size_t rem_bytes = LCD_WIDTH * LCD_HEIGHT * 2;
  while (rem_bytes > 0)
    rem_bytes -= hspi_write(&hspi, rem_bytes, pixel_buffer, 0, 0, 8, LCD_DATA);
}
