/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 "Eric Poulsen" <eric@zyxod.com>
 * Copyright (c) 2017 "Tom Manning" <tom@manningetal.com> 
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

#define MEM_MAGIC           0x75507921
/* There is 8K of rtc_slow_memory, but some is used by the system software
    If the USER_MAXLEN is set to high, the following compile error will happen:
        region `rtc_slow_seg' overflowed by N bytes
    The current system software allows almost 4096 to be used.
    To avoid running into issues if the system software uses more, 2048 was picked as a max length
*/
#define MEM_USER_MAXLEN     2048
RTC_DATA_ATTR uint32_t rtc_user_mem_magic;
RTC_DATA_ATTR uint32_t rtc_user_mem_len;
RTC_DATA_ATTR uint8_t rtc_user_mem_data[MEM_USER_MAXLEN];

// singleton RTC object
STATIC const machine_rtc_obj_t machine_rtc_obj = {{&machine_rtc_type}};

machine_rtc_config_t machine_rtc_config = { 
    .ext1_pins = 0,
    .ext0_pin = -1
    };

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

    if (rtc_user_mem_magic != MEM_MAGIC) {
        rtc_user_mem_magic = MEM_MAGIC;
        rtc_user_mem_len = 0;
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(machine_rtc_init_obj, machine_rtc_init);

STATIC mp_obj_t machine_rtc_memory(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        // read RTC memory
        uint32_t len = rtc_user_mem_len;
        uint8_t rtcram[MEM_USER_MAXLEN];
        memcpy( (char *) rtcram, (char *) rtc_user_mem_data, len);
        return mp_obj_new_bytes(rtcram,  len);
    } else {
        // write RTC memory
        mp_buffer_info_t bufinfo;
        mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);

        if (bufinfo.len > MEM_USER_MAXLEN) {
            mp_raise_ValueError("buffer too long");
        }
        memcpy( (char *) rtc_user_mem_data, (char *) bufinfo.buf, bufinfo.len);
        rtc_user_mem_len = bufinfo.len;
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_rtc_memory_obj, 1, 2, machine_rtc_memory);

STATIC const mp_map_elem_t machine_rtc_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init), (mp_obj_t)&machine_rtc_datetime_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_datetime), (mp_obj_t)&machine_rtc_datetime_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_memory), (mp_obj_t)&machine_rtc_memory_obj },
};
STATIC MP_DEFINE_CONST_DICT(machine_rtc_locals_dict, machine_rtc_locals_dict_table);

const mp_obj_type_t machine_rtc_type = {
    { &mp_type_type },
    .name = MP_QSTR_RTC,
    .make_new = machine_rtc_make_new,
    .locals_dict = (mp_obj_t)&machine_rtc_locals_dict,
};
