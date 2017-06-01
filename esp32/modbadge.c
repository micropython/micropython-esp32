
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

#include "gfx.h"
#include "board_framebuffer.h"
#include "gfxconf.h"

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

const char * font_list[] = {"Roboto-Black22","Roboto-BlackItalic24","Roboto-Regular12","Roboto-Regular18","Roboto-Regular22","PermanentMarker22"};

typedef struct _ugfx_obj_t {
    mp_obj_base_t base;
} ugfx_obj_t;

/**
 *  Badge eink hooks
 */

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

/**
 *  uGFX
 */

STATIC mp_obj_t ugfx_init(void) {
	// gwinSetDefaultFont(gdispOpenFont(font_list[0]));
	// gwinSetDefaultStyle(&WhiteWidgetStyle, FALSE);
	gfxInit();
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_init_obj, ugfx_init);

STATIC mp_obj_t ugfx_deinit(void) {
  gfxDeinit();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_deinit_obj, ugfx_deinit);

STATIC mp_obj_t ugfx_demo(mp_obj_t hacking) {
  font_t robotoBlackItalic;
  font_t permanentMarker;

  robotoBlackItalic = gdispOpenFont("Roboto_BlackItalic24");
  permanentMarker = gdispOpenFont("PermanentMarker22");

  gdispClear(White);
  gdispDrawString(150, 25, "STILL", robotoBlackItalic, Black);
  gdispDrawString(130, 50, mp_obj_str_get_str(hacking), permanentMarker, Black);
  // underline:
  gdispDrawLine(127 + 3, 50 + 22,
                127 + 3 + gdispGetStringWidth(mp_obj_str_get_str(hacking), permanentMarker) + 14, 50 + 22,
                Black);
  // cursor:
  gdispDrawLine(127 + 3 + gdispGetStringWidth(mp_obj_str_get_str(hacking), permanentMarker) + 10, 50 + 2,
                127 + 3 + gdispGetStringWidth(mp_obj_str_get_str(hacking), permanentMarker) + 10, 50 + 22 - 2,
                Black);
  gdispDrawString(140, 75, "Anyway", robotoBlackItalic, Black);
  gdispDrawCircle(60, 60, 50, Black);
  gdispDrawCircle(60, 60, 40, Black);
  gdispDrawCircle(60, 60, 30, Black);
  gdispDrawCircle(60, 60, 20, Black);
  gdispDrawCircle(60, 60, 10, Black);
  gdispFlush();

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ugfx_demo_obj, ugfx_demo);


STATIC const mp_rom_map_elem_t badge_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_badge) },

    { MP_ROM_QSTR(MP_QSTR_eink_init), MP_ROM_PTR(&badge_eink_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_display_picture), MP_ROM_PTR(&badge_display_picture_obj) },

    { MP_OBJ_NEW_QSTR(MP_QSTR_ugfx_init), (mp_obj_t)&ugfx_init_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ugfx_deinit), (mp_obj_t)&ugfx_deinit_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_ugfx_demo), (mp_obj_t)&ugfx_demo_obj },
};

STATIC MP_DEFINE_CONST_DICT(badge_module_globals, badge_module_globals_table);

const mp_obj_module_t badge_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&badge_module_globals,
};
