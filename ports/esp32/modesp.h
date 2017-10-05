#ifndef MICROPY_INCLUDED_ESP32_MODESP_H
#define MICROPY_INCLUDED_ESP32_MODESP_H

#include <stdint.h>

void esp_neopixel_write(uint8_t pin, uint8_t *pixels, uint32_t numBytes, uint8_t timing);
uint8_t temprature_sens_read(void);

#endif // MICROPY_INCLUDED_ESP32_MODESP_H
