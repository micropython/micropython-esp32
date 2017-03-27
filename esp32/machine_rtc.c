/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Josef Gajdusek
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

#include "py/nlr.h"
#include "py/obj.h"
#include "py/runtime.h"
#include "timeutils.h"
#include "modmachine.h"

typedef struct _machine_rtc_obj_t {
    mp_obj_base_t base;
} machine_rtc_obj_t;


// singleton RTC object
STATIC const machine_rtc_obj_t machine_rtc_obj = {{&machine_rtc_type}};

// Sleep time
uint64_t machine_rtc_expiry = 0; // in microseconds

STATIC mp_obj_t machine_rtc_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    // check arguments
    mp_arg_check_num(n_args, n_kw, 0, 0, false);

    // return constant object
    return (mp_obj_t)&machine_rtc_obj;
}

STATIC mp_obj_t machine_rtc_datetime(mp_uint_t n_args, const mp_obj_t *args) {
    if (n_args == 1) {
        // Get time

        struct timeval tv;
        struct tm tm;

        gettimeofday(&tv, NULL);

        gmtime_r(&tv.tv_sec, &tm);

        mp_obj_t tuple[8] = {
            mp_obj_new_int(tm.tm_year),
            mp_obj_new_int(tm.tm_mon),
            mp_obj_new_int(tm.tm_mday),
            mp_obj_new_int(tm.tm_wday),
            mp_obj_new_int(tm.tm_hour),
            mp_obj_new_int(tm.tm_min),
            mp_obj_new_int(tm.tm_sec),
            mp_obj_new_int(tv.tv_usec / 1000) // microseconds --> milliseconds
        };

        return mp_obj_new_tuple(8, tuple);
    } else {
        // Set time
        
        mp_obj_t *items;
        mp_obj_get_array_fixed_n(args[1], 8, &items);
        struct tm tm;
        struct timeval tv;

        tm.tm_year  = mp_obj_get_int(items[0]);
        tm.tm_mon   = mp_obj_get_int(items[1]);
        tm.tm_mday  = mp_obj_get_int(items[2]);
        tm.tm_hour  = mp_obj_get_int(items[4]);
        tm.tm_min   = mp_obj_get_int(items[5]);
        tm.tm_sec   = mp_obj_get_int(items[6]);

        tv.tv_sec   = mktime(&tm);
        tv.tv_usec  = mp_obj_get_int(items[7]) * 1000;

        settimeofday(&tv, NULL);

        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_rtc_datetime_obj, 1, 2, machine_rtc_datetime);

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

STATIC mp_obj_t machine_rtc_alarm(mp_obj_t self_in, mp_obj_t alarm_id, mp_obj_t time_in) {
    (void)self_in; // unused

    // check we want alarm0
    if (mp_obj_get_int(alarm_id) != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid alarm"));
    }

    // set expiry time (in microseconds)
    machine_rtc_expiry = (uint64_t)mp_obj_get_int(time_in) * 1000;

    return mp_const_none;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(machine_rtc_alarm_obj, machine_rtc_alarm);

STATIC mp_obj_t machine_rtc_irq(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // This function essentially does nothing, for backwards compatibility
    
    enum { ARG_trigger, ARG_wake };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_trigger, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
        { MP_QSTR_wake, MP_ARG_KW_ONLY | MP_ARG_INT, {.u_int = 0} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    // check we want alarm0
    if (args[ARG_trigger].u_int != 0) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "invalid alarm"));
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_rtc_irq_obj, 1, machine_rtc_irq);

STATIC const mp_map_elem_t machine_rtc_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_datetime), (mp_obj_t)&machine_rtc_datetime_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_memory), (mp_obj_t)&machine_rtc_memory_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_alarm), (mp_obj_t)&machine_rtc_alarm_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_irq), (mp_obj_t)&machine_rtc_irq_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_ALARM0), MP_OBJ_NEW_SMALL_INT(0) },
};
STATIC MP_DEFINE_CONST_DICT(machine_rtc_locals_dict, machine_rtc_locals_dict_table);

const mp_obj_type_t machine_rtc_type = {
    { &mp_type_type },
    .name = MP_QSTR_RTC,
    .make_new = machine_rtc_make_new,
    .locals_dict = (mp_obj_t)&machine_rtc_locals_dict,
};
