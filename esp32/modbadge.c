/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * SHA2017 Badge Firmware https://wiki.sha2017.org/w/Projects:Badge/MicroPython
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Paul Sokolovsky
 * Copyright (c) 2016 Damien P. George
 * Copyright (c) 2017 Anne Jan Brouwer
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

#include "modbadge.h"

#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"

// INIT

static bool badge_initialised = false;
static bool badge_leds_initialised = false;
STATIC mp_obj_t badge_init_() {
  if (!badge_initialised)
  {
      badge_initialised = true;
      badge_leds_initialised = true;
      badge_init();
  }
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_init_obj, badge_init_);

// EINK

STATIC mp_obj_t badge_eink_init_() {
  badge_eink_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_init_obj, badge_eink_init_);

/**
#define NUM_PICTURES 7
const uint8_t *pictures[NUM_PICTURES] = {
    imgv2_sha, imgv2_menu, imgv2_nick, imgv2_weather, imgv2_test, mg_logo, leaseweb
};

STATIC mp_obj_t badge_display_picture_(mp_obj_t picture_id,
                                       mp_obj_t selected_lut) {
  // TODO check for ranges
  badge_eink_display(pictures[mp_obj_get_int(picture_id)],
                     (mp_obj_get_int(selected_lut) + 1)
                         << DISPLAY_FLAG_LUT_BIT);
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(badge_display_picture_obj,
                                 badge_display_picture_);
*/

 STATIC mp_obj_t badge_eink_busy_() {
   return mp_obj_new_bool(badge_eink_dev_is_busy());
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_busy_obj, badge_eink_busy_);

 STATIC mp_obj_t badge_eink_busy_wait_() {
   badge_eink_dev_busy_wait();
   return mp_const_none;
 }
 STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_busy_wait_obj, badge_eink_busy_wait_);

// Power

STATIC mp_obj_t badge_power_init_() {
  badge_power_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_power_init_obj, badge_power_init_);

#if defined(PORTEXP_PIN_NUM_CHRGSTAT) || defined(MPR121_PIN_NUM_CHRGSTAT)
STATIC mp_obj_t battery_charge_status_() {
  return mp_obj_new_bool(badge_battery_charge_status());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(battery_charge_status_obj,
                                 battery_charge_status_);
#endif

#ifdef ADC1_CHAN_VBAT_SENSE
STATIC mp_obj_t battery_volt_sense_() {
  return mp_obj_new_int(badge_battery_volt_sense());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(battery_volt_sense_obj, battery_volt_sense_);
#endif

#ifdef ADC1_CHAN_VUSB_SENSE
STATIC mp_obj_t usb_volt_sense_() {
  return mp_obj_new_int(badge_usb_volt_sense());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(usb_volt_sense_obj, usb_volt_sense_);
#endif

// LEDs

#if defined(PIN_NUM_LED) || defined(MPR121_PIN_NUM_LEDS)
STATIC mp_obj_t badge_leds_init_() {
  if (!badge_leds_initialised) {
    badge_leds_initialised = true;
    badge_leds_init();
  }
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_leds_init_obj, badge_leds_init_);

STATIC mp_obj_t badge_leds_enable_() {
  return mp_obj_new_int(badge_leds_enable());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_leds_enable_obj, badge_leds_enable_);

STATIC mp_obj_t badge_leds_disable_() {
  return mp_obj_new_int(badge_leds_disable());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_leds_disable_obj, badge_leds_disable_);

STATIC mp_obj_t badge_leds_send_data_(mp_uint_t n_args, const mp_obj_t *args) {
  mp_uint_t len = mp_obj_get_int(args[1]);
  uint8_t *leds = (uint8_t *)mp_obj_str_get_data(args[0], &len);
  return mp_obj_new_int(badge_leds_send_data(leds, len));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_leds_send_data_obj, 2,2 ,badge_leds_send_data_);

STATIC mp_obj_t badge_leds_set_state_(mp_uint_t n_args, const mp_obj_t *args) {
  mp_uint_t len;
  uint8_t *leds = (uint8_t *)mp_obj_str_get_data(args[0], &len);
  return mp_obj_new_int(badge_leds_send_data(leds, len));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_leds_set_state_obj, 1,1 ,badge_leds_set_state_);
#endif

#if defined(PORTEXP_PIN_NUM_VIBRATOR) || defined(MPR121_PIN_NUM_VIBRATOR)
STATIC mp_obj_t badge_vibrator_init_() {
  badge_vibrator_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_vibrator_init_obj, badge_vibrator_init_);

STATIC mp_obj_t badge_vibrator_activate_(mp_uint_t n_args, const mp_obj_t *args) {
  badge_vibrator_activate(mp_obj_get_int(args[0]));
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_vibrator_activate_obj, 1,1 ,badge_vibrator_activate_);
#endif

// WIFI

#ifdef CONFIG_WIFI_USE
STATIC mp_obj_t badge_wifi_init_() {
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  wifi_config_t sta_config = {
      .sta = {.ssid = CONFIG_WIFI_SSID, .password = CONFIG_WIFI_PASSWORD, .bssid_set = false}};
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_connect());
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_wifi_init_obj, badge_wifi_init_);
#endif // CONFIG_WIFI_USE



// Module globals

STATIC const mp_rom_map_elem_t badge_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_badge)},

    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&badge_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_eink_init), MP_ROM_PTR(&badge_eink_init_obj)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_power_init), (mp_obj_t)&badge_power_init_obj},

#if defined(PIN_NUM_LED) || defined(MPR121_PIN_NUM_LEDS)
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_init), (mp_obj_t)&badge_leds_init_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_enable), (mp_obj_t)&badge_leds_enable_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_disable), (mp_obj_t)&badge_leds_disable_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_send_data), (mp_obj_t)&badge_leds_send_data_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_set_state), (mp_obj_t)&badge_leds_set_state_obj},
#endif

#if defined(PORTEXP_PIN_NUM_VIBRATOR) || defined(MPR121_PIN_NUM_VIBRATOR)
    {MP_OBJ_NEW_QSTR(MP_QSTR_vibrator_init), (mp_obj_t)&badge_vibrator_init_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_vibrator_activate), (mp_obj_t)&badge_vibrator_activate_obj},
#endif

    {MP_ROM_QSTR(MP_QSTR_eink_busy), MP_ROM_PTR(&badge_eink_busy_obj)},
    {MP_ROM_QSTR(MP_QSTR_eink_busy_wait), MP_ROM_PTR(&badge_eink_busy_wait_obj)},

/*
    {MP_ROM_QSTR(MP_QSTR_display_picture),
     MP_ROM_PTR(&badge_display_picture_obj)},
*/

#if defined(PORTEXP_PIN_NUM_CHRGSTAT) || defined(MPR121_PIN_NUM_CHRGSTAT)
    {MP_OBJ_NEW_QSTR(MP_QSTR_battery_charge_status),
     (mp_obj_t)&battery_charge_status_obj},
#endif
#ifdef ADC1_CHAN_VBAT_SENSE
    {MP_OBJ_NEW_QSTR(MP_QSTR_battery_volt_sense),
     (mp_obj_t)&battery_volt_sense_obj},
#endif
#ifdef ADC1_CHAN_VUSB_SENSE
    {MP_OBJ_NEW_QSTR(MP_QSTR_usb_volt_sense), (mp_obj_t)&usb_volt_sense_obj},
#endif

#ifdef CONFIG_WIFI_USE
  {MP_OBJ_NEW_QSTR(MP_QSTR_wifi_init), (mp_obj_t)&badge_wifi_init_obj},
#endif
};

STATIC MP_DEFINE_CONST_DICT(badge_module_globals, badge_module_globals_table);

const mp_obj_module_t badge_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&badge_module_globals,
};
