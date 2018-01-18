#ifndef _LCD_FONT_H_
#define _LCD_FONT_H_

#include <stdint.h>

#define FONT_WIDTH 5
#define FONT_HEIGHT 7
#define FONT_MARGIN 1   // vertical & horizontal
#define FIRST_CHAR 0x20 // (space)
#define CHAR_COUNT 0x61

extern const uint8_t font[];

#endif
