#ifndef BADGE_PINS_H
#define BADGE_PINS_H

#include "sdkconfig.h"

#ifdef CONFIG_SHA_BADGE_V1

// Badge revision 0.0.2
#define PIN_NUM_LED          22

#define PIN_NUM_BUTTON_A      0
#define PIN_NUM_BUTTON_B     27
#define PIN_NUM_BUTTON_MID   25
#define PIN_NUM_BUTTON_UP    26
#define PIN_NUM_BUTTON_DOWN  32
#define PIN_NUM_BUTTON_LEFT  33
#define PIN_NUM_BUTTON_RIGHT 35

#define PIN_NUM_EPD_CLK       5
#define PIN_NUM_EPD_MOSI     17
#define PIN_NUM_EPD_CS       18
#define PIN_NUM_EPD_DATA     19
#define PIN_NUM_EPD_RESET    23
#define PIN_NUM_EPD_BUSY     21

#elif defined(CONFIG_SHA_BADGE_V2)

// Badge revision 0.1.0
#define PIN_NUM_LEDS         32

#define PIN_NUM_BUTTON_FLASH  0

#define PIN_NUM_EPD_CLK      18
#define PIN_NUM_EPD_MOSI      5
#define PIN_NUM_EPD_CS       19
#define PIN_NUM_EPD_DATA     21
#define PIN_NUM_EPD_RESET    23
#define PIN_NUM_EPD_BUSY     22
#define EPD_ROTATED_180

#define PIN_NUM_I2C_CLK      27
#define PIN_NUM_I2C_DATA     26

#define PIN_NUM_PORTEXP_INT  25

#define PIN_NUM_EXT_IO_0     33
#define PIN_NUM_EXT_IO_1     16
#define PIN_NUM_EXT_IO_2     17

#define PIN_NUM_SD_CLK       14
#define PIN_NUM_SD_CMD       15
#define PIN_NUM_SD_DATA_0     2
#define PIN_NUM_SD_DATA_1     4
#define PIN_NUM_SD_DATA_2    12
#define PIN_NUM_SD_DATA_3    13

#define PIN_NUM_VUSB_SENSE   34
#define PIN_NUM_VBAT_SENSE   35

#define I2C_PORTEXP_ADDR     0x44
#define I2C_TOUCHPAD_ADDR    0x78

#define PORTEXP_PIN_NUM_CHRGSTAT 0
#define PORTEXP_PIN_NUM_VIBRATOR 1
#define PORTEXP_PIN_NUM_LEDS     2
// PORTEXP_PIN_NUM_LEDS also used for SD?
#define PORTEXP_PIN_NUM_TOUCH    3
#define PORTEXP_PIN_NUM_SD_CD    4
#define PORTEXP_PIN_NUM_EXT_IO_5 5
#define PORTEXP_PIN_NUM_EXT_IO_4 6
#define PORTEXP_PIN_NUM_EXT_IO_3 7

#else

// Badge revision 1.0.x
#define PIN_NUM_LEDS         32
#define SK6812RGBW

#define PIN_NUM_BUTTON_FLASH  0

#define PIN_NUM_EPD_CLK      18
#define PIN_NUM_EPD_MOSI      5
#define PIN_NUM_EPD_CS       19
#define PIN_NUM_EPD_DATA     21
#define PIN_NUM_EPD_RESET    23
#define PIN_NUM_EPD_BUSY     22
#define EPD_ROTATED_180

#define PIN_NUM_I2C_CLK      27
#define PIN_NUM_I2C_DATA     26
#define PIN_NUM_MPR121_INT   25

#define PIN_NUM_EXT_IO_0     33
#define PIN_NUM_EXT_IO_1     16
#define PIN_NUM_EXT_IO_2     17
#define PIN_NUM_EXT_IO_3      4
#define PIN_NUM_EXT_IO_4     12

#define PIN_NUM_SD_CLK       14
#define PIN_NUM_SD_CMD       15
#define PIN_NUM_SD_DATA_0     2
#define PIN_NUM_SD_DATA_3    13

#define PIN_NUM_VUSB_SENSE   34
#define PIN_NUM_VBAT_SENSE   35

#define I2C_MPR121_ADDR      0x5a

#define MPR121_PIN_NUM_A         0
#define MPR121_PIN_NUM_B         1
#define MPR121_PIN_NUM_START     2
#define MPR121_PIN_NUM_SELECT    3
#define MPR121_PIN_NUM_DOWN      4
#define MPR121_PIN_NUM_RIGHT     5
#define MPR121_PIN_NUM_UP        6
#define MPR121_PIN_NUM_LEFT      7
#define MPR121_PIN_NUM_VIBRATOR  8
#define MPR121_PIN_NUM_CHRGSTAT  9
#define MPR121_PIN_NUM_LEDS     10
#define MPR121_PIN_NUM_SD_CD    11

#endif

#endif // BADGE_PINS_H
