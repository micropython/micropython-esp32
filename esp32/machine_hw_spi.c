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

#include <stdio.h>
#include <stdint.h>
#include <string.h>


#include "py/runtime.h"
#include "py/stream.h"
#include "py/mphal.h"
#include "extmod/machine_spi.h"
#include "machine_hw_spi.h"
#include "modmachine.h"


// if a port didn't define MSB/LSB constants then provide them
#ifndef MICROPY_PY_MACHINE_SPI_MSB
#define MICROPY_PY_MACHINE_SPI_MSB (0)
#define MICROPY_PY_MACHINE_SPI_LSB (1)
#endif



STATIC void machine_hw_spi_transfer(mp_obj_base_t *self_in, size_t len, const uint8_t *src, uint8_t *dest) {
    machine_hw_spi_obj_t *self = MP_OBJ_TO_PTR(self_in);
    int bits_to_send = len * self->bits;
    if (self->deinitialized) {
        return;
    }

    struct spi_transaction_t transaction = {
        .flags = 0,
        .length = bits_to_send,
        .tx_buffer = NULL,
        .rx_buffer = NULL,
    };
    bool shortMsg = len <= 4;


    if(shortMsg) {
        if (src != NULL) {
            memcpy(&transaction.tx_data, src, len);
            transaction.flags |= SPI_TRANS_USE_TXDATA;
        }
        if (dest != NULL) {
            transaction.flags |= SPI_TRANS_USE_RXDATA;
        }
    } else {
        transaction.tx_buffer = src;
        transaction.rx_buffer = dest;
    }

    spi_device_transmit(self->spi, &transaction);

    if (shortMsg && dest != NULL) {
        memcpy(dest, &transaction.rx_data, len);
    }
}

/******************************************************************************/
// MicroPython bindings for hw_spi

STATIC void machine_hw_spi_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    machine_hw_spi_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "hw_spi(id=%u, baudrate=%u, polarity=%u, phase=%u, bits=%u, firstbit=%u, sck=%d, mosi=%d, miso=%d)",
    self->host, self->baudrate, self->polarity,
    self->phase, self->bits, self->firstbit,
    self->sck, self->mosi, self->miso );
}

STATIC void machine_hw_spi_init(mp_obj_base_t *self_in, size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    machine_hw_spi_obj_t *self = (machine_hw_spi_obj_t*)self_in;
    esp_err_t ret;

    enum { ARG_id, ARG_baudrate, ARG_polarity, ARG_phase, ARG_bits, ARG_firstbit, ARG_sck, ARG_mosi, ARG_miso };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_id,       MP_ARG_REQUIRED | MP_ARG_INT , {.u_int = 1} },
        { MP_QSTR_baudrate, MP_ARG_INT, {.u_int = 500000} },
        { MP_QSTR_polarity, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_phase,    MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_bits,     MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 8} },
        { MP_QSTR_firstbit, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = MICROPY_PY_MACHINE_SPI_MSB} },
        { MP_QSTR_sck,      MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_mosi,     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_miso,     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args),
    allowed_args, args);

    int host = args[ARG_id].u_int;
    if (host != HSPI_HOST && host != VSPI_HOST) {
        mp_raise_ValueError("SPI ID must be either hw_spi(1) or VSPI(2)");
    }

    self->host = host;
    self->baudrate = args[ARG_baudrate].u_int;
    self->polarity = args[ARG_polarity].u_int ? 1 : 0;
    self->phase = args[ARG_phase].u_int ? 1 : 0;
    self->bits = args[ARG_bits].u_int;
    self->firstbit = args[ARG_firstbit].u_int;
    self->sck = args[ARG_sck].u_obj == MP_OBJ_NULL ? -1 : machine_pin_get_id(args[ARG_sck].u_obj);
    self->miso = args[ARG_miso].u_obj == MP_OBJ_NULL ? -1 : machine_pin_get_id(args[ARG_miso].u_obj);
    self->mosi = args[ARG_mosi].u_obj == MP_OBJ_NULL ? -1 : machine_pin_get_id(args[ARG_mosi].u_obj);
    self->deinitialized = false;

    spi_bus_config_t buscfg = {
        .miso_io_num = self->miso,
        .mosi_io_num = self->mosi,
        .sclk_io_num = self->sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1
    };
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = self->baudrate,
        .mode = self->phase | (self->polarity << 1),
        .spics_io_num = -1, // No CS pin
        .queue_size = 1,
        .flags = self->firstbit == MICROPY_PY_MACHINE_SPI_LSB ? SPI_DEVICE_TXBIT_LSBFIRST | SPI_DEVICE_RXBIT_LSBFIRST : 0,
        .pre_cb = NULL
    };

    //Initialize the SPI bus
    // FIXME: Does the DMA matter? There are two
    ret = spi_bus_initialize(self->host, &buscfg, 1);
    assert(ret == ESP_OK);
    ret = spi_bus_add_device(self->host, &devcfg, &self->spi);
    assert(ret == ESP_OK);
}

STATIC void machine_hw_spi_deinit(mp_obj_base_t *self_in) {
    machine_hw_spi_obj_t *self = (machine_hw_spi_obj_t*)self_in;
    esp_err_t ret;
    if (!self->deinitialized) {
        self->deinitialized = true;
        ret = spi_bus_remove_device(self->spi);
        assert(ret == ESP_OK);
        ret = spi_bus_free(self->host);
        assert(ret == ESP_OK);
    }
}

mp_obj_t machine_hw_spi_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    // args[0] holds the id of the peripheral
    machine_hw_spi_obj_t *self = m_new_obj(machine_hw_spi_obj_t);
    self->base.type = &machine_hw_spi_type;
    // set defaults
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, args + n_args);
    machine_hw_spi_init((mp_obj_base_t*)self, n_args, args, &kw_args);
    return MP_OBJ_FROM_PTR(self);
}

STATIC const mp_machine_spi_p_t machine_hw_spi_p = {
    .init = machine_hw_spi_init,
    .deinit = machine_hw_spi_deinit,
    .transfer = machine_hw_spi_transfer,
};

const mp_obj_type_t machine_hw_spi_type = {
    { &mp_type_type },
    .name = MP_QSTR_hw_spi,
    .print = machine_hw_spi_print,
    .make_new = machine_hw_spi_make_new,
    .protocol = &machine_hw_spi_p,
    .locals_dict = (mp_obj_dict_t*)&mp_machine_spi_locals_dict,
};
