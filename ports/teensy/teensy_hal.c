#include <stdio.h>
#include <string.h>

#include "py/mpstate.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "usb.h"
#include "uart.h"
#include "Arduino.h"

mp_uint_t mp_hal_ticks_ms(void) {
  return millis();
}

void mp_hal_delay_ms(mp_uint_t ms) {
  delay(ms);
}

void mp_hal_set_interrupt_char(int c) {
  // The teensy 3.1 usb stack doesn't currently have the notion of generating
  // an exception when a certain character is received. That just means that
  // you can't press Control-C and get your python script to stop.
}

int mp_hal_stdin_rx_chr(void) {
    for (;;) {
        byte c;
        if (usb_vcp_recv_byte(&c) != 0) {
            return c;
        } else if (MP_STATE_PORT(pyb_stdio_uart) != NULL && uart_rx_any(MP_STATE_PORT(pyb_stdio_uart))) {
            return uart_rx_char(MP_STATE_PORT(pyb_stdio_uart));
        }
        __WFI();
    }
}

void mp_hal_stdout_tx_str(const char *str) {
    mp_hal_stdout_tx_strn(str, strlen(str));
}

void mp_hal_stdout_tx_strn(const char *str, size_t len) {
    if (MP_STATE_PORT(pyb_stdio_uart) != NULL) {
        uart_tx_strn(MP_STATE_PORT(pyb_stdio_uart), str, len);
    }
    if (usb_vcp_is_enabled()) {
        usb_vcp_send_strn(str, len);
    }
}

void mp_hal_stdout_tx_strn_cooked(const char *str, size_t len) {
    // send stdout to UART and USB CDC VCP
    if (MP_STATE_PORT(pyb_stdio_uart) != NULL) {
        uart_tx_strn_cooked(MP_STATE_PORT(pyb_stdio_uart), str, len);
    }
    if (usb_vcp_is_enabled()) {
        usb_vcp_send_strn_cooked(str, len);
    }
}

void mp_hal_gpio_clock_enable(GPIO_TypeDef *gpio) {
}

void extint_register_pin(const void *pin, uint32_t mode, int hard_irq, mp_obj_t callback_obj) {
    mp_raise_NotImplementedError(NULL);
}
