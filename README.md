# Open ESP Webradio
This project aims to build a webradio based on the ESP8266. The goal is to make
it controllable via touchscreen.

_This respository and the entire project is very much work-in-progress…_

## Hardware
The incoming encoded audio stream is buffered in an external SPI RAM. The ESP8266
has two hardware SPI controllers: SPI0 (aka SPI) and SPI1 (aka HSPI). The ESP-12F
module that is used here uses SPI0 for accessing the SPI flash memory that holds
the program data. Here, HSPI is used for accessing the SPI RAM. Since the pins
that are regularly used for HSPI will be used for talking I2S to the audio DAC,
SPI0 and HSPI are configured in overlap mode. That means, that they share the same
pins (except for the CS line). There is an arbiter that selects which SPI controller
gets to access the pins. The SPI RAM device here is a 23LC1024 which has a capacity
of 1 Mbit. Both the ESP8266 and the 23LC1024 support Quad SPI where all data lines
(SIO0..3) are bidirectional, so 4 bits are transferred in every clock cycle.

The audio DAC that is used here is the WM8731. The audio data is transferred using
I2S via the I2S controller that is built into the ESP8266. The configuration of
the DAC is performed via I2C. Even though the ESP8266 contains an I2C controller,
it is not used here, because the pins were not available. Since the WM8731 comes
with a microphone interface, it is included together with the reverse I2S direction.

The display that is used here, is a MI0283QT 240x320 color LCD. Infos about the
adapter board that molds a touchscreen controller as well, can be found
[here](https://github.com/watterott/MI0283QT-Adapter). Both the LCD controller and
the touchscreen controller are also sharing the same SPI pins as the SPI flash and
the SPI RAM. Both are controlled by the HSPI controller. The LCD uses a hardware
CS, but for the touchscreen controller, there was no more hardware CS available
that is not used for anything else. Hence, GPIO16 is used as the CS line for the
touchscreen controller.

### Schematic
![MI0283QT-Adapter](https://github.com/svenschwermer/open-esp-webradio/raw/master/hardware/schematic.png)

#### Remarks
- The SPI RAM is wired incorrectly: SIO2 and SIO3 should be swapped. This is
  fixed in software.

## Software
To build the software, follow the instructions in the
[open-esp-webradio repository](https://github.com/SuperHouse/esp-open-rtos) to
install the required toolchain. Then simply run `make` or `make flash`,
respectively, to compile or flash the image. The WiFi credentials should be
supplied via the file `./esp-open-rtos/include/private_ssid_config.h`.

## Credits
Inspired by [ESP8266_MP3_DECODER](https://github.com/espressif/ESP8266_MP3_DECODER),
this project builds upon the following components:
- [open-esp-webradio](https://github.com/SuperHouse/esp-open-rtos)
- [libmad](https://www.underbit.com/products/mad)

## License
This project is licensed under the terms of the GNU GPLv3, see [LICENSE.md](LICENSE.md).
