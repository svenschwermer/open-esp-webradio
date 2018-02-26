#ifndef _ADS7846_H_
#define _ADS7846_H_

#include <stdbool.h>
#include <stdint.h>

void ads_init(void);
bool ads_poll(uint32_t *x, uint32_t *y);
void ads_calibrate(int bottom_lines);

#endif
