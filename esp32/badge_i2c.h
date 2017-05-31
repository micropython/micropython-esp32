#ifndef BADGE_I2C_H
#define BADGE_I2C_H

#include <esp_err.h>

extern void badge_i2c_init(void);

extern esp_err_t badge_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *value);
extern esp_err_t badge_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value);
extern esp_err_t badge_i2c_read_event(uint8_t addr, uint8_t *buf);

#endif // BADGE_I2C_H
