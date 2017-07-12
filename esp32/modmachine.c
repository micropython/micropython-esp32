/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2015 Damien P. George
 * Copyright (c) 2016 Paul Sokolovsky
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

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#include "esp_system.h"

#include "py/obj.h"
#include "py/runtime.h"
#include "extmod/machine_mem.h"
#include "extmod/machine_pulse.h"
#include "extmod/machine_i2c.h"
#include "extmod/machine_spi.h"
#include "modmachine.h"
#include "esp_deep_sleep.h"
#include "mpsleep.h"
#include "machrtc.h"

#if MICROPY_PY_MACHINE

extern machine_rtc_config_t machine_rtc_config;

STATIC mp_obj_t machine_freq(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        // get
        return mp_obj_new_int(ets_get_cpu_frequency() * 1000000);
    } else {
        // set
        mp_int_t freq = mp_obj_get_int(args[0]) / 1000000;
        if (freq != 80 && freq != 160 && freq != 240) {
            nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError,
                "frequency can only be either 80Mhz, 160MHz or 240MHz"));
        }
        /*
        system_update_cpu_freq(freq);
        */
        return mp_const_none;
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(machine_freq_obj, 0, 1, machine_freq);

STATIC mp_obj_t machine_reset(void) {
    esp_restart();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_reset_obj, machine_reset);

STATIC mp_obj_t machine_unique_id(void) {
    uint8_t chipid[6];
    esp_efuse_mac_get_default(chipid);
    return mp_obj_new_bytes(chipid, 6);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_unique_id_obj, machine_unique_id);

STATIC mp_obj_t machine_idle(void) {
    taskYIELD();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_idle_obj, machine_idle);

STATIC mp_obj_t machine_disable_irq(void) {
    uint32_t state = MICROPY_BEGIN_ATOMIC_SECTION();
    return mp_obj_new_int(state);
}
MP_DEFINE_CONST_FUN_OBJ_0(machine_disable_irq_obj, machine_disable_irq);

STATIC mp_obj_t machine_enable_irq(mp_obj_t state_in) {
    uint32_t state = mp_obj_get_int(state_in);
    MICROPY_END_ATOMIC_SECTION(state);
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_1(machine_enable_irq_obj, machine_enable_irq);


//--------------------------------------------------------------------
STATIC mp_obj_t machine_deepsleep(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {

    enum {ARG_sleep_ms};
    const mp_arg_t allowed_args[] = {
        { MP_QSTR_sleep_ms, MP_ARG_INT, { .u_int = 0 } },
    };

    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);


    mp_int_t expiry = args[ARG_sleep_ms].u_int;

    if (expiry != 0) {
        esp_deep_sleep_enable_timer_wakeup(expiry * 1000);
    }

    if (machine_rtc_config.ext0_pin != -1) {
        esp_deep_sleep_enable_ext0_wakeup(machine_rtc_config.ext0_pin, machine_rtc_config.ext0_level ? 1 : 0);
    }

    if (machine_rtc_config.ext1_pins != 0) {
        esp_deep_sleep_enable_ext1_wakeup(
            machine_rtc_config.ext1_pins,
            machine_rtc_config.ext1_level ? ESP_EXT1_WAKEUP_ANY_HIGH : ESP_EXT1_WAKEUP_ALL_LOW);
    }

    if (machine_rtc_config.wake_on_touch) {
        esp_deep_sleep_enable_touchpad_wakeup();
    }

    esp_deep_sleep_start();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_KW(machine_deepsleep_obj, 0,  machine_deepsleep);

//------------------------------------------
STATIC mp_obj_t machine_wake_reason (void) {
    mpsleep_reset_cause_t reset_reason = mpsleep_get_reset_cause ();
    mpsleep_wake_reason_t wake_reason = mpsleep_get_wake_reason();
    mp_obj_t tuple[2];

    tuple[0] = mp_obj_new_int(wake_reason);
    tuple[1] = mp_obj_new_int(reset_reason);
    return mp_obj_new_tuple(2, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_wake_reason_obj, machine_wake_reason);

//----------------------------------------
STATIC mp_obj_t machine_wake_desc (void) {
    char reason[24] = { 0 };
    mp_obj_t tuple[2];

    mpsleep_get_reset_desc(reason);
    tuple[0] = mp_obj_new_str(reason, strlen(reason), 0);
    mpsleep_get_wake_desc(reason);
    tuple[1] = mp_obj_new_str(reason, strlen(reason), 0);
    return mp_obj_new_tuple(2, tuple);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(machine_wake_desc_obj, machine_wake_desc);


STATIC const mp_rom_map_elem_t machine_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_umachine) },

    { MP_ROM_QSTR(MP_QSTR_mem8), MP_ROM_PTR(&machine_mem8_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem16), MP_ROM_PTR(&machine_mem16_obj) },
    { MP_ROM_QSTR(MP_QSTR_mem32), MP_ROM_PTR(&machine_mem32_obj) },

    { MP_ROM_QSTR(MP_QSTR_freq), MP_ROM_PTR(&machine_freq_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&machine_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_unique_id), MP_ROM_PTR(&machine_unique_id_obj) },
    { MP_ROM_QSTR(MP_QSTR_idle), MP_ROM_PTR(&machine_idle_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_deepsleep), MP_ROM_PTR(&machine_deepsleep_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wake_reason), MP_ROM_PTR(&machine_wake_reason_obj) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_wake_description), MP_ROM_PTR(&machine_wake_desc_obj) },

    { MP_ROM_QSTR(MP_QSTR_disable_irq), MP_ROM_PTR(&machine_disable_irq_obj) },
    { MP_ROM_QSTR(MP_QSTR_enable_irq), MP_ROM_PTR(&machine_enable_irq_obj) },

    { MP_ROM_QSTR(MP_QSTR_time_pulse_us), MP_ROM_PTR(&machine_time_pulse_us_obj) },

    { MP_ROM_QSTR(MP_QSTR_Timer), MP_ROM_PTR(&machine_timer_type) },
    { MP_ROM_QSTR(MP_QSTR_Pin), MP_ROM_PTR(&machine_pin_type) },
    { MP_ROM_QSTR(MP_QSTR_TouchPad), MP_ROM_PTR(&machine_touchpad_type) },
    { MP_ROM_QSTR(MP_QSTR_ADC), MP_ROM_PTR(&machine_adc_type) },
    { MP_ROM_QSTR(MP_QSTR_DAC), MP_ROM_PTR(&machine_dac_type) },
    { MP_ROM_QSTR(MP_QSTR_I2C), MP_ROM_PTR(&machine_i2c_type) },
    { MP_ROM_QSTR(MP_QSTR_PWM), MP_ROM_PTR(&machine_pwm_type) },
    { MP_ROM_QSTR(MP_QSTR_SPI), MP_ROM_PTR(&mp_machine_soft_spi_type) },
    { MP_ROM_QSTR(MP_QSTR_UART), MP_ROM_PTR(&machine_uart_type) },
    { MP_OBJ_NEW_QSTR(MP_QSTR_RTC), MP_ROM_PTR(&mach_rtc_type) },
};

STATIC MP_DEFINE_CONST_DICT(machine_module_globals, machine_module_globals_table);

const mp_obj_module_t mp_module_machine = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&machine_module_globals,
};

#endif // MICROPY_PY_MACHINE
