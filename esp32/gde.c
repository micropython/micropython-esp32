#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "rom/gpio.h"
#include "sdkconfig.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"
#include "soc/gpio_struct.h"
#include "soc/spi_reg.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "gde.h"
#include "badge_pins.h"

#define SPI_NUM 0x3

#ifndef LOW
#define LOW 0
#endif
#ifndef HIGH
#define HIGH 1
#endif

#ifndef VSPICLK_OUT_IDX
// use old define
#define VSPICLK_OUT_IDX VSPICLK_OUT_MUX_IDX
#endif

void gdeReset(void) {
	gpio_set_level(PIN_NUM_EPD_RESET, LOW);
	ets_delay_us(200000);
	gpio_set_level(PIN_NUM_EPD_RESET, HIGH);
	ets_delay_us(200000);
}

bool gdeIsBusy(void) {
	return gpio_get_level(PIN_NUM_EPD_BUSY);
}

// semaphore to trigger on gde-busy signal
xSemaphoreHandle badge_gde_intr_trigger = NULL;

void gdeBusyWait(void) {
	while (gdeIsBusy()) {
		xSemaphoreTake(badge_gde_intr_trigger, 100 / portTICK_PERIOD_MS);
	}
}

#define BADGE_GDE_DEBUG
void
badge_gde_intr_handler(void *arg)
{
	int gpio_state = gpio_get_level(PIN_NUM_EPD_BUSY);
#ifdef BADGE_GDE_DEBUG
	static int gpio_last_state = -1;
	if (gpio_last_state != gpio_state)
	{
		if (gpio_state == 1)
			ets_printf("EPD-Busy Int down\n");
		else if (gpio_state == 0)
			ets_printf("EPD-Busy Int up\n");
	}
	gpio_last_state = gpio_state;
#endif // BADGE_GDE_DEBUG

#ifdef PIN_NUM_LED
	// pass on BUSY signal to LED.
	gpio_set_level(PIN_NUM_LED, 1 - gpio_state);
#endif // PIN_NUM_LED

//	if (gpio_state == 0)
//	{
		xSemaphoreGiveFromISR(badge_gde_intr_trigger, NULL);
//	}
}

void gdeWriteCommand(uint8_t command) {
	gpio_set_level(PIN_NUM_EPD_CS, HIGH);
	gpio_set_level(PIN_NUM_EPD_CS, LOW);
	gdeWriteByte(command);
	gpio_set_level(PIN_NUM_EPD_CS, HIGH);
}

void gdeWriteCommandInit(uint8_t command) {
	gpio_set_level(PIN_NUM_EPD_CS, HIGH);
	gpio_set_level(PIN_NUM_EPD_CS, LOW);
	gdeWriteByte(command);
	gpio_set_level(PIN_NUM_EPD_DATA, HIGH);
}

void gdeWriteCommandEnd(void) {
	gpio_set_level(PIN_NUM_EPD_CS, HIGH);
	gpio_set_level(PIN_NUM_EPD_DATA, LOW);
}

void gdeInit(void) {
#ifdef PIN_NUM_LED
	gpio_pad_select_gpio(PIN_NUM_LED);
	gpio_set_direction(PIN_NUM_LED, GPIO_MODE_OUTPUT);
#endif // PIN_NUM_LED

	badge_gde_intr_trigger = xSemaphoreCreateBinary();
	gpio_isr_handler_add(PIN_NUM_EPD_BUSY, badge_gde_intr_handler, NULL);
	gpio_config_t io_conf;
	io_conf.intr_type = GPIO_INTR_ANYEDGE;
	io_conf.mode = GPIO_MODE_INPUT;
	io_conf.pin_bit_mask = 1LL << PIN_NUM_EPD_BUSY;
	io_conf.pull_down_en = 0;
	io_conf.pull_up_en = 1;
	gpio_config(&io_conf);

	gpio_set_direction(PIN_NUM_EPD_CS, GPIO_MODE_OUTPUT);
	gpio_set_direction(PIN_NUM_EPD_DATA, GPIO_MODE_OUTPUT);
	gpio_set_direction(PIN_NUM_EPD_RESET, GPIO_MODE_OUTPUT);
	gpio_set_direction(PIN_NUM_EPD_BUSY, GPIO_MODE_INPUT);
	ets_printf("epd spi pin mux init ...\r\n");
	gpio_matrix_out(PIN_NUM_EPD_MOSI, VSPID_OUT_IDX, 0, 0);
	gpio_matrix_out(PIN_NUM_EPD_CLK, VSPICLK_OUT_IDX, 0, 0);
	gpio_matrix_out(PIN_NUM_EPD_CS, VSPICS0_OUT_IDX, 0, 0);
	CLEAR_PERI_REG_MASK(SPI_SLAVE_REG(SPI_NUM), SPI_TRANS_DONE << 5);
	SET_PERI_REG_MASK(SPI_USER_REG(SPI_NUM), SPI_CS_SETUP);
	CLEAR_PERI_REG_MASK(SPI_PIN_REG(SPI_NUM), SPI_CK_IDLE_EDGE);
	CLEAR_PERI_REG_MASK(SPI_USER_REG(SPI_NUM), SPI_CK_OUT_EDGE);
	CLEAR_PERI_REG_MASK(SPI_CTRL_REG(SPI_NUM), SPI_WR_BIT_ORDER);
	CLEAR_PERI_REG_MASK(SPI_CTRL_REG(SPI_NUM), SPI_RD_BIT_ORDER);
	CLEAR_PERI_REG_MASK(SPI_USER_REG(SPI_NUM), SPI_DOUTDIN);
	WRITE_PERI_REG(SPI_USER1_REG(SPI_NUM), 0);
	SET_PERI_REG_BITS(SPI_CTRL2_REG(SPI_NUM), SPI_MISO_DELAY_MODE, 0,
			SPI_MISO_DELAY_MODE_S);
	CLEAR_PERI_REG_MASK(SPI_SLAVE_REG(SPI_NUM), SPI_SLAVE_MODE);

	WRITE_PERI_REG(SPI_CLOCK_REG(SPI_NUM),
			(1 << SPI_CLKCNT_N_S) | (1 << SPI_CLKCNT_L_S)); // 40 MHz
//	WRITE_PERI_REG(SPI_CLOCK_REG(SPI_NUM), SPI_CLK_EQU_SYSCLK); // 80Mhz

	SET_PERI_REG_MASK(SPI_USER_REG(SPI_NUM),
			SPI_CS_SETUP | SPI_CS_HOLD | SPI_USR_MOSI);
	SET_PERI_REG_MASK(SPI_CTRL2_REG(SPI_NUM),
			((0x4 & SPI_MISO_DELAY_NUM) << SPI_MISO_DELAY_NUM_S));
	CLEAR_PERI_REG_MASK(SPI_USER_REG(SPI_NUM), SPI_USR_COMMAND);
	SET_PERI_REG_BITS(SPI_USER2_REG(SPI_NUM), SPI_USR_COMMAND_BITLEN, 0,
			SPI_USR_COMMAND_BITLEN_S);
	CLEAR_PERI_REG_MASK(SPI_USER_REG(SPI_NUM), SPI_USR_ADDR);
	SET_PERI_REG_BITS(SPI_USER1_REG(SPI_NUM), SPI_USR_ADDR_BITLEN, 0,
			SPI_USR_ADDR_BITLEN_S);
	CLEAR_PERI_REG_MASK(SPI_USER_REG(SPI_NUM), SPI_USR_MISO);
	SET_PERI_REG_MASK(SPI_USER_REG(SPI_NUM), SPI_USR_MOSI);

	char i;
	for (i = 0; i < 16; i++) {
		WRITE_PERI_REG((SPI_W0_REG(SPI_NUM) + (i << 2)), 0);
	}
}

void gdeWriteByte(uint8_t data) {
	SET_PERI_REG_BITS(SPI_MOSI_DLEN_REG(SPI_NUM), SPI_USR_MOSI_DBITLEN, 0x7,
			SPI_USR_MOSI_DBITLEN_S);
	WRITE_PERI_REG((SPI_W0_REG(SPI_NUM)), data);
	SET_PERI_REG_MASK(SPI_CMD_REG(SPI_NUM), SPI_USR);

	// wait until ready?
	while (READ_PERI_REG(SPI_CMD_REG(SPI_NUM)) & SPI_USR)
		;
}
