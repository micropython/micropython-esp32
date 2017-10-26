/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
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
#ifndef MICROPY_INCLUDED_STMHAL_LED_H
#define MICROPY_INCLUDED_STMHAL_LED_H

typedef enum {
    // PYBv3
    PYB_LED_R1 = 1,
    PYB_LED_R2 = 2,
    PYB_LED_G1 = 3,
    PYB_LED_G2 = 4,
    // PYBv4
    PYB_LED_RED = 1,
    PYB_LED_GREEN = 2,
    PYB_LED_YELLOW = 3,
    PYB_LED_BLUE = 4,
    //STM32F4DISC
    PYB_LED_R = 1,
    PYB_LED_G = 2,
    PYB_LED_B = 3,
    PYB_LED_O = 4,
} pyb_led_t;

void led_init(void);
void led_state(pyb_led_t led, int state);
void led_toggle(pyb_led_t led);
void led_debug(int value, int delay);

extern const mp_obj_type_t pyb_led_type;

#endif // MICROPY_INCLUDED_STMHAL_LED_H
