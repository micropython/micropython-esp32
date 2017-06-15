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

STATIC mp_obj_t badge_init_() {
  //badge_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_init_obj, badge_init_);

// EINK

STATIC mp_obj_t badge_eink_init_() {
  //badge_eink_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_init_obj, badge_eink_init_);

/*
#define NUM_PICTURES 5
const uint8_t *pictures[NUM_PICTURES] = {
    imgv2_sha, imgv2_menu, imgv2_nick, imgv2_weather, imgv2_test,
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

// Power

STATIC mp_obj_t badge_power_init_() {
  //badge_power_init();
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

STATIC mp_obj_t badge_leds_init_() {
  //badge_leds_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_leds_init_obj, badge_leds_init_);

STATIC mp_obj_t badge_leds_set_state_(mp_uint_t n_args, const mp_obj_t *args) {
  /*
  mp_uint_t len;
  uint8_t *leds = (uint8_t *)mp_obj_str_get_data(args[0], &len);
  return mp_obj_new_int(badge_leds_set_state(leds));
  */
  mp_uint_t len;
  uint8_t *rgbw = (uint8_t *)mp_obj_str_get_data(args[0], &len);
  printf("LEDs: ");
  for (int i=5; i>=0; i--) {
      uint8_t r = rgbw[i*4+0];
      uint8_t g = rgbw[i*4+1];
      uint8_t b = rgbw[i*4+2];
      printf("\x1b[48;2;%u;%u;%um", r, g, b);
      printf("\x1b[38;2;%u;%u;%um", r, g, b);
      printf(" \x1b[0m ");
  }
  printf("\n");
  return mp_obj_new_int(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_leds_set_state_obj, 1,1 ,badge_leds_set_state_);

// Module globals

STATIC const mp_rom_map_elem_t mock_badge_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_badge)},

    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&badge_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_eink_init), MP_ROM_PTR(&badge_eink_init_obj)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_power_init), (mp_obj_t)&badge_power_init_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_init), (mp_obj_t)&badge_leds_init_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_set_state), (mp_obj_t)&badge_leds_set_state_obj},

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
};

STATIC MP_DEFINE_CONST_DICT(mock_badge_module_globals, mock_badge_module_globals_table);

const mp_obj_module_t mock_badge_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&mock_badge_module_globals,
};
