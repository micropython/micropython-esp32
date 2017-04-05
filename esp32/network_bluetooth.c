/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 "Eric Poulsen" <eric@zyxod.com>
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
#include "modmachine.h"

#include "bt.h"

#include "network_bluetooth.h"


#define NETWORK_BLUETOOTH_DEBUG_PRINTF(args...) printf(args)

/******************************************************************************/
// MicroPython bindings for network_bluetooth

const mp_obj_type_t network_bluetooth_type;

STATIC void network_bluetooth_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    network_bluetooth_obj_t *self = MP_OBJ_TO_PTR(self_in);
    mp_printf(print, "Bluetooth(%p)", self);
}


STATIC mp_obj_t network_bluetooth_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *all_args) {
    NETWORK_BLUETOOTH_DEBUG_PRINTF("Entering network_bluetooth_make_new\n");
    enum { 
        ARG_id,
        /*
           ARG_bits, 
           ARG_firstbit, 
           ARG_sck, 
           ARG_mosi, 
           ARG_miso 
           */
    };
    static const mp_arg_t allowed_args[] = {
        /*
           { MP_QSTR_bits,     MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 8} },
           { MP_QSTR_firstbit, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = MICROPY_PY_MACHINE_SPI_MSB} },
           { MP_QSTR_sck,      MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
           { MP_QSTR_mosi,     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
           { MP_QSTR_miso,     MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
           */
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all_kw_array(n_args, n_kw, all_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    network_bluetooth_obj_t *self = m_new_obj(network_bluetooth_obj_t);
    self->base.type = &network_bluetooth_type;

    /*
       initialize(
       self, 
       args[ARG_id].u_int,
       args[ARG_baudrate].u_int,
       args[ARG_polarity].u_int,
       args[ARG_phase].u_int,
       args[ARG_bits].u_int,
       args[ARG_firstbit].u_int,
       args[ARG_sck].u_obj == MP_OBJ_NULL ? -1 : machine_pin_get_id(args[ARG_sck].u_obj),
       args[ARG_mosi].u_obj == MP_OBJ_NULL ? -1 : machine_pin_get_id(args[ARG_mosi].u_obj),
       args[ARG_miso].u_obj == MP_OBJ_NULL ? -1 : machine_pin_get_id(args[ARG_miso].u_obj));
       */

    return MP_OBJ_FROM_PTR(self);
}

STATIC mp_obj_t network_bluetooth_init(mp_obj_t self_in) {
    NETWORK_BLUETOOTH_DEBUG_PRINTF("Entering network_bluetooth_init\n");
    (void)self_in;
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_init_obj, network_bluetooth_init);

STATIC mp_obj_t network_bluetooth_deinit(mp_obj_t self_in) {
    NETWORK_BLUETOOTH_DEBUG_PRINTF("Entering network_bluetooth_deinit\n");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_deinit_obj, network_bluetooth_deinit);

STATIC mp_obj_t network_bluetooth_test(mp_obj_t self_in) {
    NETWORK_BLUETOOTH_DEBUG_PRINTF("Entering network_bluetooth_test\n");

    { // make_new code
        printf("Before esp_bt_controller_init()\n");
        esp_bt_controller_init(); 
        printf("After esp_bt_controller_init()\n");
        esp_err_t ret = esp_bt_controller_enable(ESP_BT_MODE_BTDM);

        switch(ret) {
            case ESP_OK:
                NETWORK_BLUETOOTH_DEBUG_PRINTF("BT initialization ok\n");
                break;
            default:
                NETWORK_BLUETOOTH_DEBUG_PRINTF("BT initialization failed, code: %d\n", ret);
        }
    }


    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(network_bluetooth_test_obj, network_bluetooth_test);

STATIC const mp_rom_map_elem_t network_bluetooth_locals_dict_table[] = {
    // instance methods
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&network_bluetooth_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_deinit), MP_ROM_PTR(&network_bluetooth_deinit_obj) },
    { MP_ROM_QSTR(MP_QSTR_test), MP_ROM_PTR(&network_bluetooth_test_obj) },

    // class constants
    /*
       { MP_ROM_QSTR(MP_QSTR_IN), MP_ROM_INT(GPIO_MODE_INPUT) },
       { MP_ROM_QSTR(MP_QSTR_OUT), MP_ROM_INT(GPIO_MODE_INPUT_OUTPUT) },
       { MP_ROM_QSTR(MP_QSTR_OPEN_DRAIN), MP_ROM_INT(GPIO_MODE_INPUT_OUTPUT_OD) },
       { MP_ROM_QSTR(MP_QSTR_PULL_UP), MP_ROM_INT(GPIO_PULLUP_ONLY) },
       { MP_ROM_QSTR(MP_QSTR_PULL_DOWN), MP_ROM_INT(GPIO_PULLDOWN_ONLY) },
       { MP_ROM_QSTR(MP_QSTR_IRQ_RISING), MP_ROM_INT(GPIO_PIN_INTR_POSEDGE) },
       { MP_ROM_QSTR(MP_QSTR_IRQ_FALLING), MP_ROM_INT(GPIO_PIN_INTR_NEGEDGE) },
       */
};

STATIC MP_DEFINE_CONST_DICT(network_bluetooth_locals_dict, network_bluetooth_locals_dict_table);

const mp_obj_type_t network_bluetooth_type = {
    { &mp_type_type },
    .name = MP_QSTR_Bluetooth,
    .print = network_bluetooth_print,
    .make_new = network_bluetooth_make_new,
    .locals_dict = (mp_obj_dict_t*)&network_bluetooth_locals_dict,
};
