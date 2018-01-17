#ifndef _MI0283QT_H_
#define _MI0283QT_H_

#include <stddef.h>
#include <stdint.h>

// values 0..63
static inline uint16_t RGB(uint16_t r, uint16_t g, uint16_t b) {
  // g2 g1 g0 b5 b4 b3 b2 b1   r5 r4 r3 r2 r1 g5 g4 g3

  return ((g & 0x07) << 15) | ((b & 0x3e) << 7) | ((r & 0x3e) << 2) |
         ((g & 0x38) >> 3);
}

int lcd_init(void);
void lcd_set_area(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void lcd_write_pixels(size_t count, const uint16_t *pixels);
void lcd_fill(uint16_t color);

#endif
