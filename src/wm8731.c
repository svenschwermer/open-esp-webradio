/*
 * wm8731.c
 *
 *  Created on: May 21, 2016
 *      Author: sven
 */

#include "i2c/i2c.h"
#include "wm8731.h"

#define I2C_BUS 0
#define I2C_SCL_PIN 5
#define I2C_SDA_PIN 4
#define I2C_ADDR 0x1a

static inline bool wm8731_write_register(const uint8_t reg, const uint8_t data)
{
	return i2c_slave_write(I2C_BUS, I2C_ADDR, &reg, &data, 1);
}

int wm8731_init()
{
	i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_100K);

	wm8731_write_register(0x1e, 0x00); // reset device
	wm8731_write_register(0x0c, 0x07); // power down control: disable DAC & output powerdown; disable poweroff
	wm8731_write_register(0x08, 0x12); // analog audio path control: mute mic & enable DAC
	wm8731_write_register(0x0a, 0x00); // digital audio path control: disable DAC soft mute
	wm8731_set_sample_rate(SAMPLE_RATE_44KHZ);
	wm8731_write_register(0x0e, 0x02); // digital audio interface format: i2s; 16 bit

	if(wm8731_write_register(0x12, 0x01)) // activate interface
		return 0;
	else
		return 1;
}

// set volume for headphone output (both channels)
// init value: 0x79 = 0 dB; 1 dB steps
// min value: 0x30 = -73 dB; <0x30 = mute
// max value: 0x7f = +6 dB
void wm8731_set_vol(uint8_t vol)
{
	wm8731_write_register(0x05, ((vol > 0x7f) ? 0x7f : vol));
}

void wm8731_set_sample_rate(enum sample_rate sample_rate)
{
	uint8_t config_val = 0x01; // USB mode
	switch(sample_rate)
	{
	case SAMPLE_RATE_44KHZ:
		config_val |= 0x22; // BOSR=1; SR=0x8
		break;
	case SAMPLE_RATE_48KHZ:
		// BOSR=0; SR=0x0
		break;
	}

	wm8731_write_register(0x10, config_val);
}
