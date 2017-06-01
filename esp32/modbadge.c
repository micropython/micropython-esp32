
#include <stdio.h>

#include "badge_eink.h"
#include "badge_i2c.h"
#include "badge_leds.h"
#include "badge_mpr121.h"
#include "badge_pins.h"
#include "badge_portexp.h"
#include "badge_touch.h"
#include "font.h"
#include "font_16px.h"
#include "font_8px.h"
#include "gde-driver.h"
#include "gde.h"
#include "imgv2_menu.h"
#include "imgv2_nick.h"
#include "imgv2_sha.h"
#include "imgv2_test.h"
#include "imgv2_weather.h"

#include "board_framebuffer.h"
#include "gfxconf.h"

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

const char * font_list[] = {"Roboto-Black22","Roboto-BlackItalic24","Roboto-Regular12","Roboto-Regular18","Roboto-Regular22","PermanentMarker22"};

STATIC mp_obj_t badge_eink_init_() {
  badge_eink_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_init_obj, badge_eink_init_);

#define NUM_PICTURES 5
const uint8_t *pictures[NUM_PICTURES] = {
	imgv2_sha,
	imgv2_menu,
	imgv2_nick,
	imgv2_weather,
	imgv2_test,
};

STATIC mp_obj_t
badge_display_picture_(mp_obj_t picture_id, mp_obj_t selected_lut)
{
  // TODO check for ranges
	badge_eink_display(pictures[mp_obj_get_int(picture_id)], (mp_obj_get_int(selected_lut)+1) << DISPLAY_FLAG_LUT_BIT);
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(badge_display_picture_obj, badge_display_picture_);

STATIC const mp_rom_map_elem_t badge_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_badge) },

    { MP_ROM_QSTR(MP_QSTR_eink_init), MP_ROM_PTR(&badge_eink_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_display_picture), MP_ROM_PTR(&badge_display_picture_obj) },
};

STATIC MP_DEFINE_CONST_DICT(badge_module_globals, badge_module_globals_table);

const mp_obj_module_t badge_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&badge_module_globals,
};
