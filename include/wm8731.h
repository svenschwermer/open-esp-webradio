#ifndef INCLUDE_DRIVER_WM8731_H_
#define INCLUDE_DRIVER_WM8731_H_

#include <stdint.h>

#define WM8731_MUTE_VOL (-74)

int wm8731_init();
int wm8731_set_vol(int vol);
int wm8731_set_sample_rate(unsigned int sample_rate);

#endif /* INCLUDE_DRIVER_WM8731_H_ */
