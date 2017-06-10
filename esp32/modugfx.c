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
#endif

#include "gfx.h"
#include "gfxconf.h"
#include "ginput_lld_toggle_config.h"

#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"

const char *font_list[] = {"Roboto-Black22",   "Roboto-BlackItalic24",
                           "Roboto-Regular12", "Roboto-Regular18",
                           "Roboto-Regular22", "PermanentMarker22"};

typedef struct _ugfx_obj_t { mp_obj_base_t base; } ugfx_obj_t;

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

// DEMO

STATIC mp_obj_t ugfx_demo(mp_obj_t hacking) {
  font_t robotoBlackItalic;
  font_t permanentMarker;

  robotoBlackItalic = gdispOpenFont("Roboto_BlackItalic24");
  permanentMarker = gdispOpenFont("PermanentMarker22");

  gdispClear(White);
  gdispDrawString(150, 25, "STILL", robotoBlackItalic, Black);
  gdispDrawString(130, 50, mp_obj_str_get_str(hacking), permanentMarker, Black);
  // underline:
  gdispDrawLine(
      127 + 3, 50 + 22,
      127 + 3 +
          gdispGetStringWidth(mp_obj_str_get_str(hacking), permanentMarker) +
          14,
      50 + 22, Black);
  // cursor:
  gdispDrawLine(
      127 + 3 +
          gdispGetStringWidth(mp_obj_str_get_str(hacking), permanentMarker) +
          10,
      50 + 2,
      127 + 3 +
          gdispGetStringWidth(mp_obj_str_get_str(hacking), permanentMarker) +
          10,
      50 + 22 - 2, Black);
  gdispDrawString(140, 75, "Anyway", robotoBlackItalic, Black);
  gdispFillCircle(60, 60, 50, Black);
  gdispFillCircle(60, 60, 40, White);
  gdispFillCircle(60, 60, 30, Black);
  gdispFillCircle(60, 60, 20, White);
  gdispFillCircle(60, 60, 10, Black);
  gdispFlush();

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(ugfx_demo_obj, ugfx_demo);

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

    {MP_OBJ_NEW_QSTR(MP_QSTR_BUTTON_UP), MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_UP)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BUTTON_DOWN),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_DOWN)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BUTTON_LEFT),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_LEFT)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BUTTON_RIGHT),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_RIGHT)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_MID), MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_MID)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_A), MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_A)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_B), MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_B)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_SELECT),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_SELECT)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_START),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_START)},
    {MP_OBJ_NEW_QSTR(MP_QSTR_BTN_FLASH),
     MP_OBJ_NEW_SMALL_INT(BADGE_BUTTON_FLASH)},

    {MP_OBJ_NEW_QSTR(MP_QSTR_clear), (mp_obj_t)&ugfx_clear_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_flush), (mp_obj_t)&ugfx_flush_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_poll), (mp_obj_t)&ugfx_poll_obj},

    {MP_OBJ_NEW_QSTR(MP_QSTR_get_string_width),
     (mp_obj_t)&ugfx_get_string_width_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_get_char_width),
     (mp_obj_t)&ugfx_get_char_width_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_char), (mp_obj_t)&ugfx_char_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_string), (mp_obj_t)&ugfx_string_obj},
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

    {MP_OBJ_NEW_QSTR(MP_QSTR_demo), (mp_obj_t)&ugfx_demo_obj},
};

STATIC MP_DEFINE_CONST_DICT(ugfx_module_globals, ugfx_module_globals_table);

const mp_obj_module_t ugfx_module = {
    .base = {&mp_type_module}, .globals = (mp_obj_dict_t *)&ugfx_module_globals,
};
