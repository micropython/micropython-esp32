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

#include <stdlib.h>
#include <stdio.h>

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

STATIC mp_obj_t badge_eink_busy_() {
  return mp_obj_new_bool(false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_busy_obj, badge_eink_busy_);

STATIC mp_obj_t badge_eink_busy_wait_() {
  // badge_eink_dev_busy_wait();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_busy_wait_obj, badge_eink_busy_wait_);

// Power

STATIC mp_obj_t badge_power_init_() {
  //badge_power_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_power_init_obj, badge_power_init_);


STATIC mp_obj_t battery_charge_status_() {
  return mp_obj_new_bool(true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(battery_charge_status_obj,
                                 battery_charge_status_);
STATIC mp_obj_t battery_volt_sense_() {
  return mp_obj_new_int(((float)rand())/RAND_MAX*1000+3500);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(battery_volt_sense_obj, battery_volt_sense_);

STATIC mp_obj_t usb_volt_sense_() {
  return mp_obj_new_int(((float)rand())/RAND_MAX*1000+4100);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(usb_volt_sense_obj, usb_volt_sense_);

// LEDs

STATIC mp_obj_t badge_leds_init_() {
  //badge_leds_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_leds_init_obj, badge_leds_init_);


STATIC mp_obj_t badge_leds_enable_() {
  //badge_leds_enable();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_leds_enable_obj, badge_leds_enable_);


STATIC mp_obj_t badge_leds_disable_() {
  //badge_leds_disable();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_leds_disable_obj, badge_leds_disable_);


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

STATIC mp_obj_t badge_leds_send_data_(mp_uint_t n_args, const mp_obj_t *args) {
  // mp_uint_t len = mp_obj_get_int(args[1]);
  // uint8_t *leds = (uint8_t *)mp_obj_str_get_data(args[0], &len);
  // return mp_obj_new_int(badge_leds_send_data(leds, len));
  mp_uint_t len = mp_obj_get_int(args[1]);
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
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_leds_send_data_obj, 2,2 ,badge_leds_send_data_);


STATIC mp_obj_t badge_vibrator_init_() {
  // badge_vibrator_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_vibrator_init_obj, badge_vibrator_init_);

STATIC mp_obj_t badge_vibrator_activate_(mp_uint_t n_args, const mp_obj_t *args) {
  int pattern = mp_obj_get_int(args[0]);
  while (pattern != 0)
	{
		if ((pattern & 1) != 0)
      printf("\a");
		pattern >>= 1;
		mp_hal_delay_ms(200);
	}
	// badge_vibrator_off();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_vibrator_activate_obj, 1,1 ,badge_vibrator_activate_);

STATIC mp_obj_t badge_wifi_init_() {
  // we just assume internets
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_wifi_init_obj, badge_wifi_init_);

// Module globals

STATIC const mp_rom_map_elem_t mock_badge_module_globals_table[] = {
  {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_badge)},

  {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&badge_init_obj)},
  {MP_ROM_QSTR(MP_QSTR_eink_init), MP_ROM_PTR(&badge_eink_init_obj)},
  {MP_OBJ_NEW_QSTR(MP_QSTR_power_init), (mp_obj_t)&badge_power_init_obj},

  {MP_OBJ_NEW_QSTR(MP_QSTR_leds_init), (mp_obj_t)&badge_leds_init_obj},
  {MP_OBJ_NEW_QSTR(MP_QSTR_leds_enable), (mp_obj_t)&badge_leds_enable_obj},
  {MP_OBJ_NEW_QSTR(MP_QSTR_leds_disable), (mp_obj_t)&badge_leds_disable_obj},
  {MP_OBJ_NEW_QSTR(MP_QSTR_leds_send_data), (mp_obj_t)&badge_leds_send_data_obj},
  {MP_OBJ_NEW_QSTR(MP_QSTR_leds_set_state), (mp_obj_t)&badge_leds_set_state_obj},

  {MP_OBJ_NEW_QSTR(MP_QSTR_vibrator_init), (mp_obj_t)&badge_vibrator_init_obj},
  {MP_OBJ_NEW_QSTR(MP_QSTR_vibrator_activate), (mp_obj_t)&badge_vibrator_activate_obj},

  {MP_ROM_QSTR(MP_QSTR_eink_busy), MP_ROM_PTR(&badge_eink_busy_obj)},
  {MP_ROM_QSTR(MP_QSTR_eink_busy_wait), MP_ROM_PTR(&badge_eink_busy_wait_obj)},

  // {MP_ROM_QSTR(MP_QSTR_display_picture),
  //  MP_ROM_PTR(&badge_display_picture_obj)},

  {MP_OBJ_NEW_QSTR(MP_QSTR_battery_charge_status),
   (mp_obj_t)&battery_charge_status_obj},
  {MP_OBJ_NEW_QSTR(MP_QSTR_battery_volt_sense),
   (mp_obj_t)&battery_volt_sense_obj},
  {MP_OBJ_NEW_QSTR(MP_QSTR_usb_volt_sense), (mp_obj_t)&usb_volt_sense_obj},

  {MP_OBJ_NEW_QSTR(MP_QSTR_wifi_init), (mp_obj_t)&badge_wifi_init_obj},
};

STATIC MP_DEFINE_CONST_DICT(mock_badge_module_globals, mock_badge_module_globals_table);

const mp_obj_module_t mock_badge_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&mock_badge_module_globals,
};
