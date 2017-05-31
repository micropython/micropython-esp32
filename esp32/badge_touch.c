#include <stdbool.h>
#include <stdint.h>

#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"

#include "rom/ets_sys.h"

#include "badge_pins.h"
#include "badge_i2c.h"
#include "badge_portexp.h"
#include "badge_touch.h"

#ifdef I2C_TOUCHPAD_ADDR

//define BADGE_TOUCH_DEBUG

badge_touch_event_t badge_touch_handler = NULL;

static inline int
badge_touch_read_event(void)
{
	uint8_t buf[3];
	esp_err_t ret = badge_i2c_read_event(I2C_TOUCHPAD_ADDR, buf);

	if (ret == ESP_OK) {
#ifdef BADGE_TOUCH_DEBUG
		ets_printf("event: 0x%02x, 0x%02x, 0x%02x\n", buf[0], buf[1], buf[2]);
#endif // BADGE_TOUCH_DEBUG
		return (buf[0] << 16) | (buf[1] << 8) | (buf[2]);
	} else {
		ets_printf("i2c master read (touch-controller): error %d\n", ret);
		return -1;
	}
}

void
badge_touch_intr_handler(void *arg)
{
	while (1)
	{
		// read touch-controller interrupt line
		int x = badge_portexp_get_input();
		if (x == -1) // error
			continue; // retry..

		if (x & (1 << PORTEXP_PIN_NUM_TOUCH)) // no events waiting
			break;

		// event waiting
		int event = badge_touch_read_event();

		if (event == -1) // error
			continue;

		if (badge_touch_handler != NULL)
			badge_touch_handler(event);
	}
}

void
badge_touch_init(void)
{
	badge_portexp_set_input_default_state(PORTEXP_PIN_NUM_TOUCH, 1);
	badge_portexp_set_interrupt_enable(PORTEXP_PIN_NUM_TOUCH, 1);
	badge_portexp_set_interrupt_handler(PORTEXP_PIN_NUM_TOUCH, badge_touch_intr_handler, NULL);

	// read pending old events
	badge_touch_intr_handler(NULL);
}

void
badge_touch_set_event_handler(badge_touch_event_t handler)
{
	badge_touch_handler = handler;
}

#endif // I2C_TOUCHPAD_ADDR
