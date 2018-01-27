#include "wm8731.h"
#include "common.h"
#include "i2c/i2c.h"

#define I2C_BUS 0
#define I2C_SCL_PIN 5
#define I2C_SDA_PIN 4
#define I2C_ADDR 0x1a

static inline int wm8731_write_register(const uint8_t reg, const uint8_t data) {
  return i2c_slave_write(I2C_BUS, I2C_ADDR, &reg, &data, 1);
}

int wm8731_init() {
  int ret;

  if ((ret = i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_100K)))
    return ret;

  static const struct {
    uint8_t reg;
    uint8_t value;
  } init_data[] = {
      {0x1e, 0x00}, // reset device
      {0x0c, 0x07}, // power down control: disable DAC & output powerdown;
                    // disable poweroff
      {0x08, 0x12}, // analog audio path control: mute mic & enable DAC
      {0x0a, 0x00}, // digital audio path control: disable DAC soft mute
      {0x0e, 0x02}, // digital audio interface format: i2s; 16 bit
  };

  for (int i = 0; i < ARRAY_SIZE(init_data); ++i) {
    if ((ret = wm8731_write_register(init_data[i].reg, init_data[i].value)))
      return ret;
  }

  if ((ret = wm8731_set_vol(-40)))
    return ret;

  if ((ret = wm8731_set_sample_rate(44100)))
    return ret;

  // activate interface
  return wm8731_write_register(0x12, 0x01);
}

// set volume for headphone output (both channels) in dB
// init value: 0 dB; min: -73 dB; max: +6 dB
int wm8731_set_vol(int vol) {
  if (vol < WM8731_MUTE_VOL)
    vol = WM8731_MUTE_VOL;
  if (vol > 6)
    vol = 6;
  vol += 73 + 0x30; // -73 dB corresponds to 0x30
  return wm8731_write_register(0x05, (uint8_t)vol);
}

int wm8731_set_sample_rate(unsigned int sample_rate) {
  uint8_t config_val = 0x01; // USB mode
  switch (sample_rate) {
  case 44100:
    config_val |= 0x22; // BOSR=1; SR=0x8
    break;
  case 48000:
    // BOSR=0; SR=0x0
    break;
  default:
    return 1;
  }

  return wm8731_write_register(0x10, config_val);
}
