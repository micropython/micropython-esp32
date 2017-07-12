/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * Modified by LoBo (https://github.com/loboris) 07/2017
 * 
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdio.h>
#include <string.h>

#include "apps/sntp/sntp.h"

#include "py/nlr.h"
#include "py/obj.h"
#include "py/objstr.h"
#include "py/runtime.h"
#include "timeutils_epoch.h"
#include "esp_system.h"
#include "machrtc.h"
#include "soc/rtc.h"
#include "esp_clk.h"

#include "sdkconfig.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "rom/uart.h"
#include "soc/soc.h"
#include "soc/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "mphalport.h"

uint32_t sntp_update_period = 3600000; // in ms

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

machine_rtc_config_t machine_rtc_config = { 0 };


#define DEFAULT_SNTP_SERVER                     "pool.ntp.org"

//------------------------------
typedef struct _mach_rtc_obj_t {
    mp_obj_base_t base;
    mp_obj_t sntp_server_name;
    bool   synced;
} mach_rtc_obj_t;

static RTC_DATA_ATTR uint64_t delta_from_epoch_til_boot;

STATIC mach_rtc_obj_t mach_rtc_obj;
const mp_obj_type_t mach_rtc_type;

//--------------------
void rtc_init0(void) {
    mach_rtc_set_us_since_epoch(0);
}

//------------------------------------------------
void mach_rtc_set_us_since_epoch(uint64_t nowus) {
    struct timeval tv;

    // store the packet timestamp
    gettimeofday(&tv, NULL);
    delta_from_epoch_til_boot = nowus - (uint64_t)((tv.tv_sec * 1000000ull) + tv.tv_usec);
}

//---------------------------
void mach_rtc_synced (void) {
    mach_rtc_obj.synced = true;
}

//------------------------------------------
uint64_t mach_rtc_get_us_since_epoch(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)((tv.tv_sec * 1000000ull) + tv.tv_usec) + delta_from_epoch_til_boot;
};

//-------------------------------------------------------------
STATIC uint64_t mach_rtc_datetime_us(const mp_obj_t datetime) {
    timeutils_struct_time_t tm;
    uint64_t useconds;

    // set date and time
    mp_obj_t *items;
    uint len;
    mp_obj_get_array(datetime, &len, &items);

    // verify the tuple
    if (len < 3 || len > 8) {
        mp_raise_ValueError("Invalid arguments");
    }

    tm.tm_year = mp_obj_get_int(items[0]);
    tm.tm_mon = mp_obj_get_int(items[1]);
    tm.tm_mday = mp_obj_get_int(items[2]);
    if (len < 7) {
        useconds = 0;
    } else {
        useconds = mp_obj_get_int(items[6]);
    }
    if (len < 6) {
        tm.tm_sec = 0;
    } else {
        tm.tm_sec = mp_obj_get_int(items[5]);
    }
    if (len < 5) {
        tm.tm_min = 0;
    } else {
        tm.tm_min = mp_obj_get_int(items[4]);
    }
    if (len < 4) {
        tm.tm_hour = 0;
    } else {
        tm.tm_hour = mp_obj_get_int(items[3]);
    }
    useconds += 1000000ull * timeutils_seconds_since_epoch(tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    return useconds;
}

/// The 8-tuple has the same format as CPython's datetime object:
///
///     (year, month, day, hours, minutes, seconds, milliseconds, tzinfo=None)
///
//------------------------------------------------------
STATIC void mach_rtc_datetime(const mp_obj_t datetime) {
    uint64_t useconds;

    if (datetime != mp_const_none) {
        useconds = mach_rtc_datetime_us(datetime);
        mach_rtc_set_us_since_epoch(useconds);
    } else {
        mach_rtc_set_us_since_epoch(0);
    }
}

/******************************************************************************/
// Micro Python bindings

//--------------------------------------------
STATIC const mp_arg_t mach_rtc_init_args[] = {
    { MP_QSTR_id,                             MP_ARG_INT, {.u_int = 0} },
    { MP_QSTR_datetime,                       MP_ARG_OBJ, {.u_obj = mp_const_none} },
    { MP_QSTR_source,                         MP_ARG_OBJ, {.u_obj = mp_const_none} },
};

//------------------------------------------------------------------------------------------------------------------------
STATIC mp_obj_t mach_rtc_make_new(const mp_obj_type_t *type, mp_uint_t n_args, mp_uint_t n_kw, const mp_obj_t *all_args) {
    // parse args
    mp_map_t kw_args;
    mp_map_init_fixed_table(&kw_args, n_kw, all_args + n_args);
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_rtc_init_args)];
    mp_arg_parse_all(n_args, all_args, &kw_args, MP_ARRAY_SIZE(args), mach_rtc_init_args, args);

    // check the peripheral id
    if (args[0].u_int != 0) {
        mp_raise_msg(&mp_type_OSError, "Resource not avaliable");
    }

    // setup the object
    mach_rtc_obj_t *self = &mach_rtc_obj;
    self->base.type = &mach_rtc_type;

    // set the time and date
    if (args[1].u_obj != mp_const_none) {
        mach_rtc_datetime(args[1].u_obj);
    }

    // return constant object
    return (mp_obj_t)&mach_rtc_obj;
}

//--------------------------------------------------------------------------------------------
STATIC mp_obj_t mach_rtc_init(mp_uint_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(mach_rtc_init_args) - 1];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(args), &mach_rtc_init_args[1], args);
    mach_rtc_datetime(args[0].u_obj);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_rtc_init_obj, 1, mach_rtc_init);

//-----------------------------------------------
STATIC mp_obj_t mach_rtc_now (mp_obj_t self_in) {
    timeutils_struct_time_t tm;
    uint64_t useconds;

    // get the time from the RTC
    useconds = mach_rtc_get_us_since_epoch();
    timeutils_seconds_since_epoch_to_struct_time((useconds / 1000000ull), &tm);

    mp_obj_t tuple[8] = {
        mp_obj_new_int(tm.tm_year),
        mp_obj_new_int(tm.tm_mon),
        mp_obj_new_int(tm.tm_mday),
        mp_obj_new_int(tm.tm_hour),
        mp_obj_new_int(tm.tm_min),
        mp_obj_new_int(tm.tm_sec),
        mp_obj_new_int(useconds % 1000000),
        mp_const_none
    };
    return mp_obj_new_tuple(8, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_rtc_now_obj, mach_rtc_now);

//---------------------------------------------------------------------------------------------
STATIC mp_obj_t mach_rtc_ntp_sync(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_server,           MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_update_period,                      MP_ARG_INT, {.u_int = 3600} },
    };

    mach_rtc_obj_t *self = MP_OBJ_TO_PTR(pos_args[0]);
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    sntp_update_period = args[1].u_int * 1000;
    if (sntp_update_period < 60000) {
        nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "update period cannot be shorter than 60 s"));
    }

    mach_rtc_obj.synced = false;
    if (sntp_enabled()) {
        sntp_stop();
    }

    self->sntp_server_name = args[0].u_obj;
    sntp_setservername(0, (char *)mp_obj_str_get_str(self->sntp_server_name));
    if (strlen(sntp_getservername(0)) == 0) {
        sntp_setservername(0, DEFAULT_SNTP_SERVER);
    }

    // set datetime to 2000/01/01 for 'synced' method to corectly detect synchronization
    mp_obj_t tuple[8] = {
        mp_obj_new_int(2000),
        mp_obj_new_int(1),
        mp_obj_new_int(1),
        mp_obj_new_int(0),
        mp_obj_new_int(0),
        mp_obj_new_int(0),
        mp_obj_new_int(0),
        mp_const_none
    };
    mach_rtc_datetime(tuple);

    sntp_init();

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(mach_rtc_ntp_sync_obj, 1, mach_rtc_ntp_sync);

//------------------------------------------------------
STATIC mp_obj_t mach_rtc_has_synced (mp_obj_t self_in) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year > (2016 - 1900)) {
        mach_rtc_obj.synced = true;
    }
    else {
        mach_rtc_obj.synced = false;
    }

    if (mach_rtc_obj.synced) {
        return mp_const_true;
    } else {
        return mp_const_false;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mach_rtc_has_synced_obj, mach_rtc_has_synced);

//----------------------------------------------------------------------------------------------------
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

//----------------------------------------------------------------------------------------------------
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


//=========================================================
STATIC const mp_map_elem_t mach_rtc_locals_dict_table[] = {
    { MP_OBJ_NEW_QSTR(MP_QSTR_init),                (mp_obj_t)&mach_rtc_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_now),                 (mp_obj_t)&mach_rtc_now_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ntp_sync),            (mp_obj_t)&mach_rtc_ntp_sync_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_synced),              (mp_obj_t)&mach_rtc_has_synced_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wake_on_ext0),        (mp_obj_t)&machine_rtc_wake_on_ext0_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wake_on_ext1),        (mp_obj_t)&machine_rtc_wake_on_ext1_obj },
};
STATIC MP_DEFINE_CONST_DICT(mach_rtc_locals_dict, mach_rtc_locals_dict_table);

//===================================
const mp_obj_type_t mach_rtc_type = {
    { &mp_type_type },
    .name = MP_QSTR_RTC,
    .make_new = mach_rtc_make_new,
    .locals_dict = (mp_obj_t)&mach_rtc_locals_dict,
};
