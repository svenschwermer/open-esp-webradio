/*
 * wm8731.h
 *
 *  Created on: May 21, 2016
 *      Author: sven
 */

#ifndef INCLUDE_DRIVER_WM8731_H_
#define INCLUDE_DRIVER_WM8731_H_

#include <stdint.h>

enum sample_rate
{
	SAMPLE_RATE_44KHZ,
	SAMPLE_RATE_48KHZ,
};

int wm8731_init();
void wm8731_set_vol(uint8_t vol);
void wm8731_set_sample_rate(enum sample_rate sample_rate);

#endif /* INCLUDE_DRIVER_WM8731_H_ */
