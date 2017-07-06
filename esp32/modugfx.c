/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * SHA2017 Badge Firmware https://wiki.sha2017.org/w/Projects:Badge/MicroPython
 *
 * Based on work by EMF for their TiLDA MK3 badge
 * https://github.com/emfcamp/micropython/tree/tilda-master/stmhal
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

#include <stdbool.h>
#include <stdio.h>

#ifndef UNIX
#include "board_framebuffer.h"
#else
#include "badge_eink.h"
uint8_t target_lut;
#endif
#include "ginput_lld_toggle_config.h"

#include <stdint.h>
#include <badge_button.h>

#include "modugfx.h"
#include "gfxconf.h"

#define MF_RLEFONT_INTERNALS
#include <mcufont.h>
#include <string.h>

#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"

#include "ugfx_widgets.h"

#define EMU_EINK_SCREEN_DELAY_MS 500

typedef struct _ugfx_obj_t { mp_obj_base_t base; } ugfx_obj_t;

static orientation_t get_orientation(int a){
	if (a == 90)
		return GDISP_ROTATE_90;
	else if (a == 180)
		return GDISP_ROTATE_180;
	else if (a == 270)
		return GDISP_ROTATE_270;
	else
		return GDISP_ROTATE_0;
}

// Our default style - a white background theme
const GWidgetStyle BWWhiteWidgetStyle = {
    White,           // window background
    Black,           // focused

    // enabled color set
    {
        Black,       // text
        Black,       // edge
        White,       // fill
        Black        // progress - active area
    },

    // disabled color set
    {
        Black,       // text
        White,       // edge
        White,       // fill
        White        // progress - active area
    },

    // pressed color set
    {
        White,       // text
        White,       // edge
        Black,       // fill
        Black        // progress - active area
    }
};


STATIC mp_obj_t ugfx_init(void) {
  gwinSetDefaultFont(gdispOpenFont(gdispListFonts()->font->short_name));
  gwinSetDefaultStyle(&BWWhiteWidgetStyle, FALSE);
  gfxInit();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_init_obj, ugfx_init);

STATIC mp_obj_t ugfx_deinit(void) {
  gfxDeinit();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_deinit_obj, ugfx_deinit);


// LUT stettings

STATIC mp_obj_t ugfx_set_lut(mp_obj_t selected_lut) {
  uint8_t lut = mp_obj_get_int(selected_lut) + 1;
  if (lut < 1 || lut > 4) {
    mp_raise_msg(&mp_type_ValueError, "invalid LUT");
  }
  target_lut = lut;
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ugfx_set_lut_obj, ugfx_set_lut);

/// \method set_orientation(a)
///
/// Set orientation to 0, 90, 180 or 270 degrees
///
STATIC mp_obj_t ugfx_set_orientation(mp_uint_t n_args, const mp_obj_t *args) {
	if (n_args > 0){
		int a = mp_obj_get_int(args[0]);
		gdispSetOrientation(get_orientation(a));

	}

    return mp_obj_new_int(gdispGetOrientation());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_set_orientation_obj, 0, 1, ugfx_set_orientation);

/// \method width()
///
/// Gets current width of the screen in pixels
///
STATIC mp_obj_t ugfx_width(void) {
    return mp_obj_new_int(gdispGetWidth());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_width_obj, ugfx_width);



/// \method height()
///
/// Gets current width of the screen in pixels
///
STATIC mp_obj_t ugfx_height(void) {
    return mp_obj_new_int(gdispGetHeight());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_height_obj, ugfx_height);


/// \method get_pixel()
///
/// Gets the colour of the given pixel at (x,y)
///
STATIC mp_obj_t ugfx_get_pixel(mp_obj_t x_in, mp_obj_t y_in) {
    // extract arguments
    //ugfx_obj_t *self = args[0];
	//int x = mp_obj_get_int(x_in);
	//int y = mp_obj_get_int(y_in);
	return mp_obj_new_int(0);
	//needs sorting, currently returns somewhat dodgy values
    //return mp_obj_new_int(gdispGetPixelColor(x,y));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(ugfx_get_pixel_obj, ugfx_get_pixel);


/// \method set_default_font()
///
/// Sets the default font used by widgets.
/// Note, it is only necessary to use a font object if font scaling is used, since
///  in this case memory will need to be cleared once the scaled font is no longer required
///
STATIC mp_obj_t ugfx_set_default_font(mp_obj_t font_obj) {
	ugfx_font_obj_t *fo = font_obj;
	if (MP_OBJ_IS_TYPE(font_obj, &ugfx_font_type)){
		gwinSetDefaultFont(fo->font);
	}else if (MP_OBJ_IS_STR(font_obj)){
		const char *file = mp_obj_str_get_str(font_obj);
		gwinSetDefaultFont(gdispOpenFont(file));
        /*}else if (MP_OBJ_IS_INT(font_obj)){*/
        /*if (mp_obj_get_int(font_obj) < sizeof(font_list)/sizeof(char*)){*/
        /*gwinSetDefaultFont(gdispOpenFont(font_list[mp_obj_get_int(font_obj)]));*/
        /*}*/
        /*else*/
        /*nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Invalid font index"));*/
        /*}*/
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ugfx_set_default_font_obj, ugfx_set_default_font);

/// \method set_default_style()
///
/// Sets the default style used by widgets.
///
STATIC mp_obj_t ugfx_set_default_style(mp_obj_t style_obj) {
	ugfx_style_obj_t *st = style_obj;
	if (MP_OBJ_IS_TYPE(style_obj, &ugfx_style_type))
		gwinSetDefaultStyle(&(st->style),FALSE);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ugfx_set_default_style_obj, ugfx_set_default_style);

/// \method send_tab()
///
/// Sends a 'tab' signal to cycle through focus.
///
STATIC mp_obj_t ugfx_send_tab(void) {

	GSourceListener	*psl=0;
	GEventKeyboard	*pe;

	while ((psl = geventGetSourceListener(ginputGetKeyboard(GKEYBOARD_ALL_INSTANCES), psl))){
		pe = (GEventKeyboard *)geventGetEventBuffer(psl);


		pe->type = GEVENT_KEYBOARD;
		pe->bytecount = 1;
		pe->c[0] = GKEY_TAB;
		geventSendEvent(psl);
	}

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_send_tab_obj, ugfx_send_tab);


// PRIMITIVES

/// \method clear(color=ugfx.WHITE)
///
/// Clear screen
///
STATIC mp_obj_t ugfx_clear(mp_uint_t n_args, const mp_obj_t *args) {
  int color = n_args == 0 ? White : mp_obj_get_int(args[0]);
  gdispFillArea(0, 0, gdispGetWidth(), gdispGetHeight(), color);
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_clear_obj, 0, 1, ugfx_clear);

/// \method flush()
///
/// Flush the display buffer to the screen
///
STATIC mp_obj_t ugfx_flush() {
#ifdef UNIX
  mp_hal_delay_ms(EMU_EINK_SCREEN_DELAY_MS);
#endif
  gdispFlush();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_flush_obj, ugfx_flush);

/// \method get_char_width(char, font)
///
/// Get length in pixels of given character font combination.
///
STATIC mp_obj_t ugfx_get_char_width(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  mp_uint_t len;
  const uint16_t data = mp_obj_get_int(args[0]);
  const char *font = mp_obj_str_get_data(args[1], &len);

  return mp_obj_new_int(gdispGetCharWidth(data, gdispOpenFont(font)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_get_char_width_obj, 2, 2,
                                           ugfx_get_char_width);

/// \method get_string_width(str, font)
///
/// Get length in pixels of given text font combination.
///
STATIC mp_obj_t ugfx_get_string_width(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  mp_uint_t len;
  const char *data = mp_obj_str_get_data(args[0], &len);
  const char *font = mp_obj_str_get_data(args[1], &len);

  return mp_obj_new_int(gdispGetStringWidth(data, gdispOpenFont(font)));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_get_string_width_obj, 2, 2,
                                           ugfx_get_string_width);

/// \method char(x, y, char, font, colour)
///
/// Draw the given character to the position `(x, y)` using the given font and
/// colour.
///
STATIC mp_obj_t ugfx_char(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  mp_uint_t len;
  const uint16_t data = mp_obj_get_int(args[2]);
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int col = mp_obj_get_int(args[4]);
  const char *font = mp_obj_str_get_data(args[3], &len);

  gdispDrawChar(x0, y0, data, gdispOpenFont(font), col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_char_obj, 5, 5, ugfx_char);

/// \method text(x, y, str, colour)
///
/// Draw the given text to the position `(x, y)` using the given colour.
///
STATIC mp_obj_t ugfx_text(mp_uint_t n_args, const mp_obj_t *args) {
    // extract arguments
    //ugfx_obj_t *self = args[0];
    mp_uint_t len;
    const char *data = mp_obj_str_get_data(args[2], &len);
    int x0 = mp_obj_get_int(args[0]);
    int y0 = mp_obj_get_int(args[1]);
    int col = mp_obj_get_int(args[3]);

    gdispDrawString(x0, y0, data, gwinGetDefaultFont(), col);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_text_obj, 4, 4, ugfx_text);

/// \method string(x, y, str, font, colour)
///
/// Draw the given text to the position `(x, y)` using the given font and
/// colour.
///
STATIC mp_obj_t ugfx_string(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  mp_uint_t len;
  const char *data = mp_obj_str_get_data(args[2], &len);
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int col = mp_obj_get_int(args[4]);
  const char *font = mp_obj_str_get_data(args[3], &len);

  gdispDrawString(x0, y0, data, gdispOpenFont(font), col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_string_obj, 5, 5, ugfx_string);

/// \method string_box(x, y, a, b, str, font, colour, justify)
///
/// Draw the given text in a box at position `(x, y)` with lengths a, b
/// using the given font and colour.
///
STATIC mp_obj_t ugfx_string_box(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  mp_uint_t len;
  const char *data = mp_obj_str_get_data(args[4], &len);
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int x1 = mp_obj_get_int(args[2]);
  int y1 = mp_obj_get_int(args[3]);
  int col = mp_obj_get_int(args[6]);
  const char *font = mp_obj_str_get_data(args[5], &len);
  justify_t justify = mp_obj_get_int(args[7]);

  gdispDrawStringBox(x0, y0, x1, y1, data, gdispOpenFont(font), col, justify);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_string_box_obj, 8, 8,
                                           ugfx_string_box);

/// \method pixel(x, y, colour)
///
/// Draw a pixel at (x,y) using the given colour.
///
STATIC mp_obj_t ugfx_pixel(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int col = mp_obj_get_int(args[2]);

  gdispDrawPixel(x0, y0, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_pixel_obj, 3, 3, ugfx_pixel);

/// \method line(x1, y1, x2, y2, colour)
///
/// Draw a line from (x1,y1) to (x2,y2) using the given colour.
///
STATIC mp_obj_t ugfx_line(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int x1 = mp_obj_get_int(args[2]);
  int y1 = mp_obj_get_int(args[3]);
  int col = mp_obj_get_int(args[4]);

  gdispDrawLine(x0, y0, x1, y1, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_line_obj, 5, 5, ugfx_line);

/// \method thickline(x1, y1, x2, y2, colour, width, round)
///
/// Draw a line with a given thickness from (x1,y1) to (x2,y2) using the given
/// colour. Option to round the ends
///
STATIC mp_obj_t ugfx_thickline(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int x1 = mp_obj_get_int(args[2]);
  int y1 = mp_obj_get_int(args[3]);
  int col = mp_obj_get_int(args[4]);
  int width = mp_obj_get_int(args[5]);
  bool rnd = (mp_obj_get_int(args[6]) != 0);

  gdispDrawThickLine(x0, y0, x1, y1, col, width, rnd);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_thickline_obj, 7, 7,
                                           ugfx_thickline);

/// \method arc(x1, y1, r, angle1, angle2, colour)
///
/// Draw an arc having a centre point at (x1,y1), radius r, using the given
/// colour.
///
STATIC mp_obj_t ugfx_arc(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int r = mp_obj_get_int(args[2]);
  int col = mp_obj_get_int(args[5]);
  int a1 = mp_obj_get_int(args[3]);
  int a2 = mp_obj_get_int(args[4]);

  gdispDrawArc(x0, y0, r, a1, a2, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_arc_obj, 6, 6, ugfx_arc);

/// \method fill_arc(x1, y1, r, angle1, angle2, colour)
///
/// Fill an arc having a centre point at (x1,y1), radius r, using the given
/// colour.
///
STATIC mp_obj_t ugfx_fill_arc(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int r = mp_obj_get_int(args[2]);
  int col = mp_obj_get_int(args[5]);
  int a1 = mp_obj_get_int(args[3]);
  int a2 = mp_obj_get_int(args[4]);

  gdispFillArc(x0, y0, r, a1, a2, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_fill_arc_obj, 6, 6,
                                           ugfx_fill_arc);

/// \method circle(x1, y1, r, colour)
///
/// Draw a circle having a centre point at (x1,y1), radius r, using the given
/// colour.
///
STATIC mp_obj_t ugfx_circle(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int r = mp_obj_get_int(args[2]);
  int col = mp_obj_get_int(args[3]);

  gdispDrawCircle(x0, y0, r, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_circle_obj, 4, 4, ugfx_circle);

/// \method fill_circle(x1, y1, r, colour)
///
/// Fill a circle having a centre point at (x1,y1), radius r, using the given
/// colour.
///
STATIC mp_obj_t ugfx_fill_circle(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int r = mp_obj_get_int(args[2]);
  int col = mp_obj_get_int(args[3]);

  gdispFillCircle(x0, y0, r, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_fill_circle_obj, 4, 4,
                                           ugfx_fill_circle);

/// \method ellipse(x1, y1, a, b, colour)
///
/// Draw a ellipse having a centre point at (x1,y1), lengths a,b, using the
/// given colour.
///
STATIC mp_obj_t ugfx_ellipse(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int a = mp_obj_get_int(args[2]);
  int b = mp_obj_get_int(args[3]);
  int col = mp_obj_get_int(args[4]);

  gdispDrawEllipse(x0, y0, a, b, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_ellipse_obj, 5, 5,
                                           ugfx_ellipse);

/// \method fill_ellipse(x1, y1, a, b, colour)
///
/// Fill a ellipse having a centre point at (x1,y1), lengths a,b, using the
/// given colour.
///
STATIC mp_obj_t ugfx_fill_ellipse(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int a = mp_obj_get_int(args[2]);
  int b = mp_obj_get_int(args[3]);
  int col = mp_obj_get_int(args[4]);

  gdispFillEllipse(x0, y0, a, b, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_fill_ellipse_obj, 5, 5,
                                           ugfx_fill_ellipse);

/// \method polygon(x1, y1, array, colour)
///
/// Draw a polygon starting at (x1,y1), using the array of points, using the
/// given colour.
///
STATIC mp_obj_t ugfx_polygon(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int col = mp_obj_get_int(args[3]);

  point ar[20];

  mp_obj_t *mp_arr;
  mp_obj_t *mp_arr2;
  size_t len;
  size_t len2;
  mp_obj_get_array(args[2], &len, &mp_arr);

  if (len <= 20) {
    int i, j;
    j = 0;
    for (i = 0; i < len; i++) {
      mp_obj_get_array(mp_arr[i], &len2, &mp_arr2);
      if (len2 == 2) {
        point p = {mp_obj_get_int(mp_arr2[0]), mp_obj_get_int(mp_arr2[1])};
        ar[j++] = p;
      }
    }
    gdispDrawPoly(x0, y0, ar, j, col);
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_polygon_obj, 4, 4,
                                           ugfx_polygon);

/// \method fill_polygon(x1, y1, array, colour)
///
/// Fill a polygon starting at (x1,y1), using the array of points, using the
/// given colour.
///
STATIC mp_obj_t ugfx_fill_polygon(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int col = mp_obj_get_int(args[3]);

  point ar[20];

  mp_obj_t *mp_arr;
  mp_obj_t *mp_arr2;
  size_t len;
  size_t len2;
  mp_obj_get_array(args[2], &len, &mp_arr);

  if (len <= 20) {
    int i, j;
    j = 0;
    for (i = 0; i < len; i++) {
      mp_obj_get_array(mp_arr[i], &len2, &mp_arr2);
      if (len2 == 2) {
        point p = {mp_obj_get_int(mp_arr2[0]), mp_obj_get_int(mp_arr2[1])};
        ar[j++] = p;
      }
    }
    gdispFillConvexPoly(x0, y0, ar, i, col);
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_fill_polygon_obj, 4, 4,
                                           ugfx_fill_polygon);

/// \method area(x, y, a, b, colour)
///
/// Fill area from (x,y), with lengths a,b, using the given colour.
///
STATIC mp_obj_t ugfx_area(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int a = mp_obj_get_int(args[2]);
  int b = mp_obj_get_int(args[3]);
  int col = mp_obj_get_int(args[4]);

  gdispFillArea(x0, y0, a, b, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_area_obj, 5, 5, ugfx_area);

/// \method box(x, y, a, b, colour)
///
/// Draw a box from (x,y), with lengths a,b, using the given colour.
///
STATIC mp_obj_t ugfx_box(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int a = mp_obj_get_int(args[2]);
  int b = mp_obj_get_int(args[3]);
  int col = mp_obj_get_int(args[4]);

  gdispDrawBox(x0, y0, a, b, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_box_obj, 5, 5, ugfx_box);

/// \method rounded_box(x, y, a, b, colour)
///
/// Draw a box from (x,y), with lengths a,b, rounded corners with radius r,
/// using the given colour.
///
STATIC mp_obj_t ugfx_rounded_box(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int a = mp_obj_get_int(args[2]);
  int b = mp_obj_get_int(args[3]);
  int r = mp_obj_get_int(args[4]);
  int col = mp_obj_get_int(args[5]);

  gdispDrawRoundedBox(x0, y0, a, b, r, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_rounded_box_obj, 6, 6,
                                           ugfx_rounded_box);

/// \method fill_rounded_box(x, y, a, b, colour)
///
/// Draw a box from (x,y), with lengths a,b, rounded corners with radius r,
/// using the given colour.
///
STATIC mp_obj_t ugfx_fill_rounded_box(mp_uint_t n_args, const mp_obj_t *args) {
  // extract arguments
  // ugfx_obj_t *self = args[0];
  int x0 = mp_obj_get_int(args[0]);
  int y0 = mp_obj_get_int(args[1]);
  int a = mp_obj_get_int(args[2]);
  int b = mp_obj_get_int(args[3]);
  int r = mp_obj_get_int(args[4]);
  int col = mp_obj_get_int(args[5]);

  gdispFillRoundedBox(x0, y0, a, b, r, col);

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_fill_rounded_box_obj, 6, 6,
                                           ugfx_fill_rounded_box);

// Image


/// \method display_image(x, y, image_object)
///
STATIC mp_obj_t ugfx_display_image(mp_uint_t n_args, const mp_obj_t *args){
    // extract arguments
    //pyb_ugfx_obj_t *self = args[0];
	int x = mp_obj_get_int(args[0]);
	int y = mp_obj_get_int(args[1]);
	mp_obj_t img_obj = args[2];
	gdispImage imo;
	gdispImage *iptr;

	if (img_obj != mp_const_none) {
		if (MP_OBJ_IS_STR(img_obj)){
			const char *img_str = mp_obj_str_get_str(img_obj);
			gdispImageError er = gdispImageOpenFile(&imo, img_str);
			if (er != 0){
				nlr_raise(mp_obj_new_exception_msg(&mp_type_ValueError, "Error opening file"));
				return mp_const_none;
			}
			iptr = &imo;
		}
		else if (MP_OBJ_IS_TYPE(img_obj, &ugfx_image_type))
			iptr = &(((ugfx_image_obj_t*)img_obj)->thisImage);
		else{
            nlr_raise(mp_obj_new_exception_msg(&mp_type_TypeError, "img argument needs to be be a Image or String type"));
			return mp_const_none;
		}


		coord_t	swidth, sheight;

		// Get the display dimensions
		swidth = gdispGetWidth();
		sheight = gdispGetHeight();

		// if (n_args > 3)
		// 	set_blit_rotation(get_orientation(mp_obj_get_int(args[3])));

		int err = gdispImageDraw(iptr, x, y, swidth, sheight, 0, 0);

		// set_blit_rotation(GDISP_ROTATE_0);

		if (MP_OBJ_IS_STR(img_obj))
			gdispImageClose(&imo);

		print_image_error(err);  // TODO

	}

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_display_image_obj, 3, 3, ugfx_display_image);

// INPUT

/// \method poll()
///
/// calls gfxYield, which will handle widget redrawing when for inputs.
/// Register as follows:
/// tim = pyb.Timer(3)
/// tim.init(freq=60)
/// tim.callback(lambda t:ugfx.poll())
///
STATIC mp_obj_t ugfx_poll(void) {
  gfxYield();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_poll_obj, ugfx_poll);

// callback system

STATIC mp_obj_t button_callbacks[1+BADGE_BUTTONS];
STATIC GListener button_listeners[1+BADGE_BUTTONS];

void ugfx_ginput_callback_handler(void *param, GEvent *pe){
  size_t button = (size_t) param;
  if(button_callbacks[button] != mp_const_none){
    GEventToggle *toggle = (GEventToggle*) pe;
    mp_sched_schedule(button_callbacks[button], mp_obj_new_bool(toggle->on ? 1 : 0));
  }
}

/// \method ugfx_input_init()
///
/// Enable callbacks for button events
///
STATIC mp_obj_t ugfx_input_init(void) {
  for(size_t i = 1; i <= BADGE_BUTTONS; i++){
    button_callbacks[i] = mp_const_none;
    geventListenerInit(&button_listeners[i]);
    button_listeners[i].callback = ugfx_ginput_callback_handler;
    button_listeners[i].param = (void*) i;
    geventAttachSource(&button_listeners[i], ginputGetToggle(i), GLISTEN_TOGGLE_ON|GLISTEN_TOGGLE_OFF);
  }
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_input_init_obj, ugfx_input_init);

/// \method ugfx_input_attach(button, callback)
///
/// Register callbacks for button events. This overwrites any previous callback registered for this button.
///
STATIC mp_obj_t ugfx_input_attach(mp_uint_t n_args, const mp_obj_t *args) {
  uint8_t button = mp_obj_get_int(args[0]);
  button_callbacks[button] = args[1];
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_input_attach_obj, 2, 2, ugfx_input_attach);

// DEMO

STATIC mp_obj_t ugfx_demo(mp_obj_t hacking) {
  font_t robotoBlackItalic;
  font_t permanentMarker;

  robotoBlackItalic = gdispOpenFont("Roboto_BlackItalic24");
  permanentMarker = gdispOpenFont("PermanentMarker36");

  uint16_t hackingWidth = gdispGetStringWidth(mp_obj_str_get_str(hacking), permanentMarker);

  gdispClear(White);
  gdispDrawStringBox(0,  6, 296, 40, "STILL", robotoBlackItalic, Black, justifyCenter);
  gdispDrawStringBox(0, 42, 276, 40, mp_obj_str_get_str(hacking), permanentMarker, Black, justifyCenter);
  // underline:
  gdispDrawLine(
    296/2 - hackingWidth/2 - 14,
    42 + 36 + 2,
    296/2 + hackingWidth/2 + 14,
    42 + 36 + 2,
    Black);
  // cursor:
  gdispDrawLine(
    276/2 + hackingWidth/2 + 2 + 3,
    42 + 4,
    276/2 + hackingWidth/2 + 2,
    42 + 36,
    Black);
  gdispDrawStringBox(0, 82, 296, 40, "Anyway", robotoBlackItalic, Black, justifyCenter);
#ifdef UNIX
  mp_hal_delay_ms(EMU_EINK_SCREEN_DELAY_MS);
#endif
  gdispFlush();

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ugfx_demo_obj, ugfx_demo);

STATIC mp_obj_t ugfx_fonts_list(void) {
  mp_obj_t list = mp_obj_new_list(0, NULL);;
  const struct mf_font_list_s *f = gdispListFonts();

  while (f){
    mp_obj_list_append(list, mp_obj_new_str(f->font->short_name, strlen(f->font->short_name), false));
    f = f->next;
  }

  return list;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(ugfx_fonts_list_obj, ugfx_fonts_list);

#define store_dict_str(dict, field, contents) mp_obj_dict_store(dict, mp_obj_new_str(field, strlen(field), false), mp_obj_new_str(contents, strlen(contents), false))
#define store_dict_int(dict, field, contents) mp_obj_dict_store(dict, mp_obj_new_str(field, strlen(field), false), mp_obj_new_int(contents));
#define store_dict_foo(dict, field, contents) mp_obj_dict_store(dict, mp_obj_new_str(field, strlen(field), false), contents);

STATIC mp_obj_t ugfx_fonts_dump(mp_uint_t n_args, const mp_obj_t *args) {
  mp_uint_t len;
  const char *font = mp_obj_str_get_data(args[0], &len);
  struct mf_rlefont_s* f = (struct mf_rlefont_s*) gdispOpenFont(font);
  if(strcmp(f->font.short_name, font)){
    return mp_const_none;
  }

  mp_obj_t dict_out   = mp_obj_new_dict(0);
  mp_obj_dict_t *dict = MP_OBJ_TO_PTR(dict_out);

  mp_obj_dict_init(dict, 21);
  store_dict_str(dict, "short_name", f->font.short_name);
  store_dict_str(dict, "full_name", f->font.full_name);
  store_dict_int(dict, "width", f->font.width);
  store_dict_int(dict, "height", f->font.height);
  store_dict_int(dict, "min_x_advance", f->font.min_x_advance);
  store_dict_int(dict, "max_x_advance", f->font.max_x_advance);
  store_dict_int(dict, "baseline_x", f->font.baseline_x);
  store_dict_int(dict, "baseline_y", f->font.baseline_y);
  store_dict_int(dict, "line_height", f->font.line_height);
  store_dict_int(dict, "flags", f->font.flags);
  store_dict_int(dict, "fallback_character", f->font.fallback_character);

  mp_obj_t ranges = mp_obj_new_list(0, NULL);;

  for (size_t i = 0; i < f->char_range_count; i++){
	const struct mf_rlefont_char_range_s *range = &f->char_ranges[i];
	mp_obj_t range_dict_out   = mp_obj_new_dict(0);
	mp_obj_dict_t *range_dict = MP_OBJ_TO_PTR(range_dict_out);
	store_dict_int(range_dict, "first_char", range->first_char);
	store_dict_int(range_dict, "char_count", range->char_count);
	store_dict_foo(range_dict, "glyph_offsets", mp_obj_new_bytes((uint8_t*) range->glyph_offsets, 2*range->char_count+2));
	store_dict_foo(range_dict, "glyph_data", mp_obj_new_bytes(range->glyph_data, (range->glyph_offsets[range->char_count])));
    mp_obj_list_append(ranges, range_dict_out);
  }

  store_dict_foo(dict, "dictionary_offsets", mp_obj_new_bytes((uint8_t*) f->dictionary_offsets, 2*(f->dict_entry_count)+2));
  store_dict_foo(dict, "dictionary_data",    mp_obj_new_bytes(f->dictionary_data, f->dictionary_offsets[f->dict_entry_count]));

  store_dict_int(dict, "rle_entry_count", f->rle_entry_count);
  store_dict_int(dict, "dict_entry_count", f->dict_entry_count);
  store_dict_int(dict, "char_range_count", f->char_range_count);

  store_dict_foo(dict, "char_ranges", ranges);

  return dict_out;

}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_fonts_dump_obj, 1, 1, ugfx_fonts_dump);

STATIC mp_obj_t ugfx_fonts_load(mp_uint_t n_args, const mp_obj_t *args) {

  struct dynamic_mf_rlefont_char_range_s{
    uint16_t first_char;
    uint16_t char_count;
    uint16_t *glyph_offsets;
    uint8_t *glyph_data;
  };

  struct dynamic_mf_font_s{
    const char *full_name;
    const char *short_name;
    uint8_t width;
    uint8_t height;
    uint8_t min_x_advance;
    uint8_t max_x_advance;
    uint8_t baseline_x;
    uint8_t baseline_y;
    uint8_t line_height;
    uint8_t flags;
    uint16_t fallback_character;
    uint8_t (*character_width)(const struct mf_font_s *font, uint16_t character);
    uint8_t (*render_character)(const struct mf_font_s *font,
                                int16_t x0, int16_t y0,
                                uint16_t character,
                                mf_pixel_callback_t callback,
                                void *state);
  };

  struct dynamic_mf_rlefont_s{
    struct dynamic_mf_font_s font;
    uint8_t version;
    uint8_t *dictionary_data;
    uint16_t *dictionary_offsets;
    uint8_t rle_entry_count;
    uint8_t dict_entry_count;
    uint16_t char_range_count;
    struct dynamic_mf_rlefont_char_range_s *char_ranges;
  };

  mp_obj_dict_t *dict = MP_OBJ_TO_PTR(args[0]);

  struct dynamic_mf_rlefont_s* f = gfxAlloc(sizeof(struct mf_rlefont_s));
  if(f){
    f->font.short_name         = mp_obj_str_get_str(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_short_name)));
    if(gdispOpenFont(f->font.short_name)->short_name == f->font.short_name){
      // font already loaded
      return mp_obj_new_bool(0);
    }
    f->font.full_name          = mp_obj_str_get_str(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_full_name)));
    f->font.width              = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_width)));
    f->font.height             = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_height)));
    f->font.min_x_advance      = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_min_x_advance)));
    f->font.max_x_advance      = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_max_x_advance)));
    f->font.baseline_x         = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_baseline_x)));
    f->font.baseline_y         = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_baseline_y)));
    f->font.line_height        = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_line_height)));
    f->font.flags              = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_flags))) | 0xc0; // FONT_FLAG_DYNAMIC|FONT_FLAG_UNLISTED
    f->font.fallback_character = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_fallback_character)));

    f->rle_entry_count         = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_rle_entry_count)));
    f->dict_entry_count        = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_dict_entry_count)));
    f->char_range_count        = mp_obj_get_int(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_char_range_count)));

    f->char_ranges = gfxAlloc(f->char_range_count * sizeof(struct mf_rlefont_s));
    mp_obj_t ranges = mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_char_ranges));
    size_t len;
    uint8_t * data;
    for (size_t i = 0; i < f->char_range_count; i++){
      mp_obj_t * items;
      size_t idx;
      mp_obj_list_get(ranges, &idx, &items);
      mp_obj_t range = items[0];
      f->char_ranges[i].first_char = mp_obj_get_int(mp_obj_dict_get(range, MP_OBJ_NEW_QSTR(MP_QSTR_first_char)));
      f->char_ranges[i].char_count = mp_obj_get_int(mp_obj_dict_get(range, MP_OBJ_NEW_QSTR(MP_QSTR_char_count)));

      data                            = (uint8_t*) mp_obj_str_get_data(mp_obj_dict_get(range, MP_OBJ_NEW_QSTR(MP_QSTR_glyph_offsets)), &len);
      f->char_ranges[i].glyph_offsets = gfxAlloc(len);
      memcpy(f->char_ranges[i].glyph_offsets, data, len-2);

      data                            = (uint8_t*) mp_obj_str_get_data(mp_obj_dict_get(range, MP_OBJ_NEW_QSTR(MP_QSTR_glyph_data)), &len);
      f->char_ranges[i].glyph_offsets[f->char_ranges[i].char_count] = len;

      f->char_ranges[i].glyph_data    = gfxAlloc(len);
      memcpy(f->char_ranges[i].glyph_data, data, len);
    }

    data                            = (uint8_t*) mp_obj_str_get_data(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_dictionary_offsets)), &len);
    f->dictionary_offsets           = gfxAlloc(len);
    memcpy(f->dictionary_offsets, data, len-2);

    size_t dict_len;
    data                             = (uint8_t*) mp_obj_str_get_data(mp_obj_dict_get(dict, MP_OBJ_NEW_QSTR(MP_QSTR_dictionary_data)), &dict_len);
    f->dictionary_offsets[(len/2)-1] = dict_len;
    f->dictionary_data               = gfxAlloc(dict_len);
    memcpy(f->dictionary_data, data, dict_len);

    f->font.character_width  = &mf_rlefont_character_width;
    f->font.render_character = &mf_rlefont_render_character;

    gdispAddFont((font_t)f);
  }

  return mp_obj_new_bool(1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ugfx_fonts_load_obj, 1, 1, ugfx_fonts_load);

// Module globals

STATIC const mp_rom_map_elem_t ugfx_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ugfx)},

    {MP_OBJ_NEW_QSTR(MP_QSTR_init), (mp_obj_t)&ugfx_init_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_deinit), (mp_obj_t)&ugfx_deinit_obj},

    {MP_OBJ_NEW_QSTR(MP_QSTR_BLACK), MP_OBJ_NEW_SMALL_INT(Black)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_WHITE), MP_OBJ_NEW_SMALL_INT(White)},

    {MP_OBJ_NEW_QSTR(MP_QSTR_justifyLeft), MP_OBJ_NEW_SMALL_INT(justifyLeft)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_justifyCenter),
     MP_OBJ_NEW_SMALL_INT(justifyCenter)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_justifyRight), MP_OBJ_NEW_SMALL_INT(justifyRight)},

    {MP_OBJ_NEW_QSTR(MP_QSTR_JOY_UP), MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_UP)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_JOY_DOWN),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_DOWN)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_JOY_LEFT),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_LEFT)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_JOY_RIGHT),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_RIGHT)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_A), MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_A)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_B), MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_B)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_SELECT),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_SELECT)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_START),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_START)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_FLASH),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_FLASH)},

     {MP_OBJ_NEW_QSTR(MP_QSTR_LUT_FULL), MP_OBJ_NEW_SMALL_INT(BADGE_EINK_LUT_FULL)},
     {MP_OBJ_NEW_QSTR(MP_QSTR_LUT_NORMAL), MP_OBJ_NEW_SMALL_INT(BADGE_EINK_LUT_NORMAL)},
     {MP_OBJ_NEW_QSTR(MP_QSTR_LUT_FASTER), MP_OBJ_NEW_SMALL_INT(BADGE_EINK_LUT_FASTER)},
		 {MP_OBJ_NEW_QSTR(MP_QSTR_LUT_FASTEST), MP_OBJ_NEW_SMALL_INT(BADGE_EINK_LUT_FASTEST)},
		 {MP_OBJ_NEW_QSTR(MP_QSTR_LUT_DEFAULT), MP_OBJ_NEW_SMALL_INT(BADGE_EINK_LUT_DEFAULT)},
		 {MP_OBJ_NEW_QSTR(MP_QSTR_LUT_MAX), MP_OBJ_NEW_SMALL_INT(BADGE_EINK_LUT_MAX)},

     {MP_OBJ_NEW_QSTR(MP_QSTR_set_lut), (mp_obj_t)&ugfx_set_lut_obj},

    {MP_OBJ_NEW_QSTR(MP_QSTR_clear), (mp_obj_t)&ugfx_clear_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_flush), (mp_obj_t)&ugfx_flush_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_poll), (mp_obj_t)&ugfx_poll_obj},

    {MP_OBJ_NEW_QSTR(MP_QSTR_get_string_width),
     (mp_obj_t)&ugfx_get_string_width_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_get_char_width),
     (mp_obj_t)&ugfx_get_char_width_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_char), (mp_obj_t)&ugfx_char_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_string), (mp_obj_t)&ugfx_string_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_text), (mp_obj_t)&ugfx_text_obj },
    {MP_OBJ_NEW_QSTR(MP_QSTR_string_box), (mp_obj_t)&ugfx_string_box_obj},

    {MP_OBJ_NEW_QSTR(MP_QSTR_pixel), (mp_obj_t)&ugfx_pixel_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_line), (mp_obj_t)&ugfx_line_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_box), (mp_obj_t)&ugfx_box_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_rounded_box), (mp_obj_t)&ugfx_rounded_box_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_fill_rounded_box),
     (mp_obj_t)&ugfx_fill_rounded_box_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_area), (mp_obj_t)&ugfx_area_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_thickline), (mp_obj_t)&ugfx_thickline_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_circle), (mp_obj_t)&ugfx_circle_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_fill_circle), (mp_obj_t)&ugfx_fill_circle_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_ellipse), (mp_obj_t)&ugfx_ellipse_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_fill_ellipse), (mp_obj_t)&ugfx_fill_ellipse_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_arc), (mp_obj_t)&ugfx_arc_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_fill_arc), (mp_obj_t)&ugfx_fill_arc_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_polygon), (mp_obj_t)&ugfx_polygon_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_fill_polygon), (mp_obj_t)&ugfx_fill_polygon_obj},

    { MP_OBJ_NEW_QSTR(MP_QSTR_display_image), (mp_obj_t)&ugfx_display_image_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_orientation), (mp_obj_t)&ugfx_set_orientation_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_width), (mp_obj_t)&ugfx_width_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_height), (mp_obj_t)&ugfx_height_obj },

    {MP_OBJ_NEW_QSTR(MP_QSTR_fonts_list), (mp_obj_t)&ugfx_fonts_list_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_fonts_dump), (mp_obj_t)&ugfx_fonts_dump_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_fonts_load), (mp_obj_t)&ugfx_fonts_load_obj},

    {MP_OBJ_NEW_QSTR(MP_QSTR_input_init), (mp_obj_t)&ugfx_input_init_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_input_attach), (mp_obj_t)&ugfx_input_attach_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_demo), (mp_obj_t)&ugfx_demo_obj},
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_default_font), (mp_obj_t)&ugfx_set_default_font_obj },
    { MP_OBJ_NEW_QSTR(MP_QSTR_set_default_style), (mp_obj_t)&ugfx_set_default_style_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_send_tab), (mp_obj_t)&ugfx_send_tab_obj },

    { MP_OBJ_NEW_QSTR(MP_QSTR_Button), (mp_obj_t)&ugfx_button_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Container), (mp_obj_t)&ugfx_container_type },
    // { MP_OBJ_NEW_QSTR(MP_QSTR_Graph), (mp_obj_t)&ugfx_graph_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Font), (mp_obj_t)&ugfx_font_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_List), (mp_obj_t)&ugfx_list_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Textbox), (mp_obj_t)&ugfx_textbox_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Style), (mp_obj_t)&ugfx_style_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Keyboard), (mp_obj_t)&ugfx_keyboard_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Label), (mp_obj_t)&ugfx_label_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Image), (mp_obj_t)&ugfx_image_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Checkbox), (mp_obj_t)&ugfx_checkbox_type },
    { MP_OBJ_NEW_QSTR(MP_QSTR_Imagebox), (mp_obj_t)&ugfx_imagebox_type },
};

STATIC MP_DEFINE_CONST_DICT(ugfx_module_globals, ugfx_module_globals_table);

const mp_obj_module_t ugfx_module = {
    .base = {&mp_type_module}, .globals = (mp_obj_dict_t *)&ugfx_module_globals,
};
