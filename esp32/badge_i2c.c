#include <stdbool.h>
#include <stdint.h>

#include <stdio.h>
#include <string.h>
#include "driver/i2c.h"

#include "sdkconfig.h"

#include "badge_pins.h"
#include "badge_i2c.h"

#ifdef PIN_NUM_I2C_CLK

//define BADGE_I2C_DEBUG

#define I2C_MASTER_NUM             I2C_NUM_1
#define I2C_MASTER_FREQ_HZ         100000
//define I2C_MASTER_FREQ_HZ        400000
#define I2C_MASTER_TX_BUF_DISABLE  0
#define I2C_MASTER_RX_BUF_DISABLE  0

#define WRITE_BIT      I2C_MASTER_WRITE /*!< I2C master write */
#define READ_BIT       I2C_MASTER_READ  /*!< I2C master read */
#define ACK_CHECK_EN   0x1     /*!< I2C master will check ack from slave */
#define ACK_CHECK_DIS  0x0     /*!< I2C master will not check ack from slave */
#define ACK_VAL        0x0     /*!< I2C ack value */
#define NACK_VAL       0x1     /*!< I2C nack value */

// mutex for accessing the I2C bus
xSemaphoreHandle badge_i2c_mux = NULL;

void
badge_i2c_init(void)
{
	// create mutex for I2C bus
	badge_i2c_mux = xSemaphoreCreateMutex();

	// configure I2C
	i2c_config_t conf;
	memset(&conf, 0, sizeof(i2c_config_t));
	conf.mode = I2C_MODE_MASTER;
	conf.sda_io_num = PIN_NUM_I2C_DATA;
	conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
//	conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
	conf.scl_io_num = PIN_NUM_I2C_CLK;
	conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
//	conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
	conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
	i2c_param_config(I2C_MASTER_NUM, &conf);

	i2c_driver_install(I2C_MASTER_NUM, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}

int
badge_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *value)
{
	xSemaphoreTake(badge_i2c_mux, portMAX_DELAY);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( addr << 1 ) | READ_BIT, ACK_CHECK_EN);
	i2c_master_read_byte(cmd, value, NACK_VAL);
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	xSemaphoreGive(badge_i2c_mux);

	return ret;
}

int
badge_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t value)
{
	xSemaphoreTake(badge_i2c_mux, portMAX_DELAY);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( addr << 1 ) | WRITE_BIT, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
	i2c_master_write_byte(cmd, value, ACK_CHECK_EN);
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	xSemaphoreGive(badge_i2c_mux);

	return ret;
}

int
badge_i2c_read_event(uint8_t addr, uint8_t *buf)
{
	xSemaphoreTake(badge_i2c_mux, portMAX_DELAY);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, ( addr << 1 ) | READ_BIT, ACK_CHECK_EN);
	i2c_master_read(cmd, buf, 3, ACK_VAL);
	i2c_master_stop(cmd);

	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	xSemaphoreGive(badge_i2c_mux);

	return ret;
}

#endif // PIN_NUM_I2C_CLK
