/*
 * This file is part of the MicroPython project, http://micropython.org/
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

#ifndef MICROPY_INCLUDED_MACHINE_HW_SPI_H
#define MICROPY_INCLUDED_MACHINE_HW_SPI_H

#include "driver/spi_master.h"

typedef struct _machine_hw_spi_obj_t {
    mp_obj_base_t base;
    spi_host_device_t host;
    uint32_t baudrate;
    uint8_t polarity;
    uint8_t phase;
    uint8_t bits;
    uint8_t firstbit;
    int8_t sck;
    int8_t mosi;
    int8_t miso;
    spi_device_handle_t spi;
    enum {
        MACHINE_HW_SPI_STATE_NONE, 
        MACHINE_HW_SPI_STATE_INIT, 
        MACHINE_HW_SPI_STATE_DEINIT} state;
} machine_hw_spi_obj_t;

extern const mp_obj_type_t machine_hw_spi_type ;

#endif // MICROPY_INCLUDED_MACHINE_HW_SPI_H
