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
#include <string.h>

#include <time.h>
#include <sys/time.h>
#include "driver/gpio.h"

#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "timeutils.h"
#include "modmachine.h"
#include "machine_rtc.h"

typedef struct _machine_rtc_obj_t {
    mp_obj_base_t base;
} machine_rtc_obj_t;

#define MACHINE_RTC_VALID_EXT_PINS \
( \
    (1ll << 0)  | \
    (1ll << 2)  | \
    (1ll << 4)  | \
    (1ll << 12) | \
    (1ll << 13) | \
    (1ll << 14) | \
    (1ll << 15) | \
    (1ll << 25) | \
    (1ll << 26) | \
    (1ll << 27) | \
    (1ll << 32) | \
    (1ll << 33) | \
    (1ll << 34) | \
    (1ll << 35) | \
    (1ll << 36) | \
    (1ll << 37) | \
    (1ll << 38) | \
    (1ll << 39)   \
)

#define MACHINE_RTC_LAST_EXT_PIN 39

// singleton RTC object
STATIC const machine_rtc_obj_t machine_rtc_obj = {{&machine_rtc_type}};

machine_rtc_config_t machine_rtc_config = { 0 };

STATIC mp_obj_t machine_rtc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    // check arguments
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    // return constant object
    return (mp_obj_t)&machine_rtc_obj;
}

STATIC mp_obj_t machine_rtc_datetime_helper(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        // Get time

        struct timeval tv;
        struct tm tm;

        gettimeofday(&tv, NULL);

        gmtime_r(&tv.tv_sec, &tm);

        mp_obj_t tuple[8] = {
            mp_obj_new_int(tm.tm_year + 1900),
            mp_obj_new_int(tm.tm_mon + 1),
            mp_obj_new_int(tm.tm_mday),
            mp_obj_new_int(tm.tm_wday + 1),
            mp_obj_new_int(tm.tm_hour),
            mp_obj_new_int(tm.tm_min),
            mp_obj_new_int(tm.tm_sec),
            mp_obj_new_int(tv.tv_usec)
        };

        return mp_obj_new_tuple(8, tuple);
    } else {
        // Set time

        mp_obj_t *items;
        mp_obj_get_array_fixed_n(args[1], 8, &items);
        struct tm tm;
        struct timeval tv;

        tm.tm_year  = mp_obj_get_int(items[0]) - 1900;
        tm.tm_mon   = mp_obj_get_int(items[1]) - 1;
        tm.tm_mday  = mp_obj_get_int(items[2]);
        tm.tm_hour  = mp_obj_get_int(items[4]);
        tm.tm_min   = mp_obj_get_int(items[5]);
        tm.tm_sec   = mp_obj_get_int(items[6]);

        tv.tv_sec   = mktime(&tm);
        tv.tv_usec  = mp_obj_get_int(items[7]);

        settimeofday(&tv, NULL);

        return mp_const_none;
    }
}
STATIC mp_obj_t machine_rtc_datetime(mp_uint_t n_args, const mp_obj_t *args) {
    return machine_rtc_datetime_helper(n_args, args);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_rtc_datetime_obj, 1, 2, machine_rtc_datetime);

STATIC mp_obj_t machine_rtc_init(mp_obj_t self_in, mp_obj_t date) {
    mp_obj_t args[2] = {self_in, date};
    machine_rtc_datetime_helper(2, args);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(machine_rtc_init_obj, machine_rtc_init);

STATIC mp_obj_t machine_rtc_memory(mp_uint_t n_args, const mp_obj_t *args) {

    if (n_args == 1) {
        // read RTC memory

        // FIXME; need to update IDF

        return mp_const_none;
    } else {
        // write RTC memory

        // FIXME; need to update IDF

        return mp_const_none;
    }

}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_rtc_memory_obj, 1, 2, machine_rtc_memory);

STATIC mp_obj_t machine_rtc_wake_on_touch(mp_obj_t self_in, const mp_obj_t wake) {
    (void)self_in; // unused

    machine_rtc_config.wake_on_touch = mp_obj_is_true(wake);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(machine_rtc_wake_on_touch_obj, machine_rtc_wake_on_touch);

STATIC mp_obj_t machine_rtc_wake_on_ext0(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum {ARG_pin, ARG_level};
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_pin,  MP_ARG_OBJ, {.u_obj = mp_obj_new_int(machine_rtc_config.ext0_pin)} },
        { MP_QSTR_level,  MP_ARG_BOOL, {.u_bool = machine_rtc_config.ext0_level} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    if (args[ARG_pin].u_obj == mp_const_none) {
        machine_rtc_config.ext0_pin = -1; // "None"
    } else {
        gpio_num_t pin_id = machine_pin_get_id(args[ARG_pin].u_obj);
        if (pin_id != machine_rtc_config.ext0_pin) {
            if (!((1ll << pin_id) & MACHINE_RTC_VALID_EXT_PINS)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid pin"));
            }
            machine_rtc_config.ext0_pin = pin_id;
        }
    }

    machine_rtc_config.ext0_level = args[ARG_level].u_bool;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_rtc_wake_on_ext0_obj, 1, machine_rtc_wake_on_ext0);

STATIC mp_obj_t machine_rtc_wake_on_ext1(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum {ARG_pins, ARG_level};
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_pins, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_level, MP_ARG_BOOL, {.u_bool = machine_rtc_config.ext1_level} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    uint64_t ext1_pins = machine_rtc_config.ext1_pins;


    // Check that all pins are allowed
    if (args[ARG_pins].u_obj != mp_const_none) {
        mp_uint_t len = 0;
        mp_obj_t *elem;
        mp_obj_get_array(args[ARG_pins].u_obj, &len, &elem);
        ext1_pins = 0;

        for (int i = 0; i < len; i++) {

            gpio_num_t pin_id = machine_pin_get_id(elem[i]);
            // mp_int_t pin = mp_obj_get_int(elem[i]);
            uint64_t pin_bit = (1ll << pin_id);

            if (!(pin_bit & MACHINE_RTC_VALID_EXT_PINS)) {
                nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid pin"));
                break;
            }
            ext1_pins |= pin_bit;
        }
    }

    machine_rtc_config.ext1_level = args[ARG_level].u_bool;
    machine_rtc_config.ext1_pins = ext1_pins;

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_rtc_wake_on_ext1_obj, 1, machine_rtc_wake_on_ext1);

STATIC const mp_map_elem_t machine_rtc_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init), (mp_obj_t)&machine_rtc_datetime_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_datetime), (mp_obj_t)&machine_rtc_datetime_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_memory), (mp_obj_t)&machine_rtc_memory_obj },
    // FIXME -- need to enable touch IRQ for this to work,
    // doesn't seem possible in the IDF presently.
    // { MP_OBJ_NEW_QSTR(MP_QSTR_wake_on_touch), (mp_obj_t)&machine_rtc_wake_on_touch_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wake_on_ext0), (mp_obj_t)&machine_rtc_wake_on_ext0_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wake_on_ext1), (mp_obj_t)&machine_rtc_wake_on_ext1_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WAKEUP_ALL_LOW), mp_const_false },
    { MP_OBJ_NEW_QSTR(MP_QSTR_WAKEUP_ANY_HIGH), mp_const_true },
};
STATIC MP_DEFINE_CONST_DICT(machine_rtc_locals_dict, machine_rtc_locals_dict_table);

const mp_obj_type_t machine_rtc_type = {
    { &mp_type_type },
    .name = MP_QSTR_RTC,
    .make_new = machine_rtc_make_new,
    .locals_dict = (mp_obj_t)&machine_rtc_locals_dict,
};
