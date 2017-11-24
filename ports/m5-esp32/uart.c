/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>

#include "driver/uart.h"
#include "driver/gpio.h"

#include "py/mpstate.h"
#include "py/mphal.h"
#include "i2c_keyboard.h"

STATIC void uart_irq_handler(void *arg);
// STATIC void gpio_isr_handler(void *arg);


#define KEYBOARD_ISR_PIN     5
#define GPIO_INPUT_PIN_SEL  (1<<KEYBOARD_ISR_PIN)
#define ESP_INTR_FLAG_DEFAULT 0

// static void IRAM_ATTR gpio_isr_handler(void* arg) {
//     uint32_t gpio_num = *((uint32_t*)arg);
//     if(gpio_num == KEYBOARD_ISR_PIN) 
//     {
//         int i2c_keychar = i2c_keyboard_read();
//         ringbuf_put(&stdin_ringbuf, i2c_keychar);
//     }
// }

void i2c_keyboard_handle(void* arg) {
    for(;;) {
        if(gpio_get_level(KEYBOARD_ISR_PIN) == 0) {
            int i2c_keychar = i2c_keyboard_read();
            ringbuf_put(&stdin_ringbuf, i2c_keychar);
            // printf("i2c_keyboard_read:0x%x \r\n", i2c_keychar);
        }
        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

void gpio_isr_init() {

    gpio_config_t io_conf;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
    //bit mask of the pins, use GPIO5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode    
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    // //change gpio intrrupt type for one pin
    // gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    // //create a queue to handle gpio event from isr
    // gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    // xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);


    // //install gpio isr service
    // gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    // //remove isr handler for gpio number.
    // gpio_isr_handler_remove(KEYBOARD_ISR_PIN);
    // //hook isr handler for specific gpio pin
    // gpio_isr_handler_add(KEYBOARD_ISR_PIN, gpio_isr_handler, (void*) KEYBOARD_ISR_PIN);
}


void uart_init(void) {
    uart_isr_handle_t handle;
    uart_isr_register(UART_NUM_0, uart_irq_handler, NULL, ESP_INTR_FLAG_LOWMED | ESP_INTR_FLAG_IRAM, &handle);
    uart_enable_rx_intr(UART_NUM_0);

#if MICROPY_PY_LCD_TERMINAL
    gpio_isr_init();
    xTaskCreate(i2c_keyboard_handle, "i2c_keyboard_handle", 2048, NULL, 10, NULL);
#endif
}

// all code executed in ISR must be in IRAM, and any const data must be in DRAM
STATIC void IRAM_ATTR uart_irq_handler(void *arg) {
    volatile uart_dev_t *uart = &UART0;
    uart->int_clr.rxfifo_full = 1;
    uart->int_clr.frm_err = 1;
    uart->int_clr.rxfifo_tout = 1;
    while (uart->status.rxfifo_cnt) {
        uint8_t c = uart->fifo.rw_byte;
        if (c == mp_interrupt_char) {
            // inline version of mp_keyboard_interrupt();
            MP_STATE_VM(mp_pending_exception) = MP_OBJ_FROM_PTR(&MP_STATE_VM(mp_kbd_exception));
            #if MICROPY_ENABLE_SCHEDULER
            if (MP_STATE_VM(sched_state) == MP_SCHED_IDLE) {
                MP_STATE_VM(sched_state) = MP_SCHED_PENDING;
            }
            #endif
        } else {
            // this is an inline function so will be in IRAM
            ringbuf_put(&stdin_ringbuf, c);
        }
    }
}
