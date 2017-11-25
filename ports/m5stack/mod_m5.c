/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Paul Sokolovsky
 * Copyright (c) 2016 Damien P. George
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

#include "rom/gpio.h"
#include "tft.h"

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"

// #define U32toColor(x)  (*((color_t*)(&x)))

inline color_t U32toColor(uint32_t c) {
    c = ((uint8_t*)&c)[0]<<16 | ((uint8_t*)&c)[1] <<8 | ((uint8_t*)&c)[2];
    return (*((color_t*)(&c)));
}

STATIC mp_obj_t M5_init() {
    TFT_init();
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(M5_init_obj, M5_init);

STATIC mp_obj_t M5_TFT_drawPixel(mp_obj_t x, mp_obj_t y, mp_obj_t color) {
    int16_t gx = mp_obj_get_int(x);
    int16_t gy = mp_obj_get_int(y);
    uint32_t gc = mp_obj_get_int(color);
    TFT_drawPixel(gx, gy, U32toColor(gc), 1);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(M5_TFT_drawPixel_obj, M5_TFT_drawPixel);

STATIC mp_obj_t M5_TFT_fillScreen(mp_obj_t c) {
    uint32_t gc = mp_obj_get_int(c);
    TFT_fillScreen(U32toColor(gc));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(M5_TFT_fillScreen_obj, M5_TFT_fillScreen);

STATIC mp_obj_t M5_TFT_drawRect(size_t n_args, const mp_obj_t *args) {
    
    int16_t gx = mp_obj_get_int(args[0]);
    int16_t gy = mp_obj_get_int(args[1]);
    int16_t gw = mp_obj_get_int(args[2]);
    int16_t gh = mp_obj_get_int(args[3]);
    uint32_t gcolor = mp_obj_get_int(args[4]);
    TFT_drawRect(gx, gy, gw, gh, U32toColor(gcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(M5_TFT_drawRect_obj, 5, 5, M5_TFT_drawRect);

STATIC mp_obj_t M5_TFT_fillRect(size_t n_args, const mp_obj_t *args) {
    
    int16_t gx = mp_obj_get_int(args[0]);
    int16_t gy = mp_obj_get_int(args[1]);
    int16_t gw = mp_obj_get_int(args[2]);
    int16_t gh = mp_obj_get_int(args[3]);
    uint32_t gcolor = mp_obj_get_int(args[4]);
    TFT_fillRect(gx, gy, gw, gh, U32toColor(gcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(M5_TFT_fillRect_obj, 5, 5, M5_TFT_fillRect);

STATIC mp_obj_t M5_TFT_drawLine(size_t n_args, const mp_obj_t *args) {
    
    int16_t x0 = mp_obj_get_int(args[0]);
    int16_t y0 = mp_obj_get_int(args[1]);
    int16_t x1 = mp_obj_get_int(args[2]);
    int16_t y1 = mp_obj_get_int(args[3]);
    uint32_t gcolor = mp_obj_get_int(args[4]);
    TFT_drawLine(x0, y0, x1, y1, U32toColor(gcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(M5_TFT_drawLine_obj, 5, 5, M5_TFT_drawLine);

STATIC mp_obj_t M5_TFT_drawTriangle(size_t n_args, const mp_obj_t *args) {
    
    int16_t x0 = mp_obj_get_int(args[0]);
    int16_t y0 = mp_obj_get_int(args[1]);
    int16_t x1 = mp_obj_get_int(args[2]);
    int16_t y1 = mp_obj_get_int(args[3]);
    int16_t x2 = mp_obj_get_int(args[4]);
    int16_t y2 = mp_obj_get_int(args[5]);
    uint32_t gcolor = mp_obj_get_int(args[6]);
    TFT_drawTriangle(x0, y0, x1, y1, x2, y2, U32toColor(gcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(M5_TFT_drawTriangle_obj, 7, 7, M5_TFT_drawTriangle);

STATIC mp_obj_t M5_TFT_fillTriangle(size_t n_args, const mp_obj_t *args) {
    
    int16_t x0 = mp_obj_get_int(args[0]);
    int16_t y0 = mp_obj_get_int(args[1]);
    int16_t x1 = mp_obj_get_int(args[2]);
    int16_t y1 = mp_obj_get_int(args[3]);
    int16_t x2 = mp_obj_get_int(args[4]);
    int16_t y2 = mp_obj_get_int(args[5]);
    uint32_t gcolor = mp_obj_get_int(args[6]);
    TFT_fillTriangle(x0, y0, x1, y1, x2, y2, U32toColor(gcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(M5_TFT_fillTriangle_obj, 7, 7, M5_TFT_fillTriangle);

STATIC mp_obj_t M5_TFT_drawCircle(size_t n_args, const mp_obj_t *args) {
    
    int16_t x = mp_obj_get_int(args[0]);
    int16_t y = mp_obj_get_int(args[1]);
    int16_t radius = mp_obj_get_int(args[2]);
    uint32_t gcolor = mp_obj_get_int(args[3]);
    TFT_drawCircle(x, y, radius, U32toColor(gcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(M5_TFT_drawCircle_obj, 4, 4, M5_TFT_drawCircle);

STATIC mp_obj_t M5_TFT_fillCircle(size_t n_args, const mp_obj_t *args) {
    
    int16_t x = mp_obj_get_int(args[0]);
    int16_t y = mp_obj_get_int(args[1]);
    int16_t radius = mp_obj_get_int(args[2]);
    uint32_t gcolor = mp_obj_get_int(args[3]);
    TFT_fillCircle(x, y, radius, U32toColor(gcolor));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(M5_TFT_fillCircle_obj, 4, 4, M5_TFT_fillCircle);

STATIC mp_obj_t M5_TFT_print(mp_obj_t str, mp_obj_t x, mp_obj_t y) {
    const char *strc = mp_obj_str_get_str(str);
    int xx = mp_obj_get_int(x);
    int yy = mp_obj_get_int(y);

    TFT_print((char*)strc, (int)(xx), (int)(yy));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(M5_TFT_print_obj, M5_TFT_print);


STATIC mp_obj_t M5_term_stdin(mp_obj_t str) {
    const char* instr = mp_obj_str_get_str(str);
    while(*instr) ringbuf_put(&stdin_ringbuf, *instr++);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(M5_term_stdin_obj, M5_term_stdin);

STATIC mp_obj_t M5_termout_getch() {
    uint8_t buffer[255];
    uint8_t index = 0;
    int c;

    for(index=0; index<254; index++) {
        c = ringbuf_get(&stdout_ringbuf);
        buffer[index] = (uint8_t)c;
        if(c == -1) {
            break;
        }
    }
    buffer[index] = 0;
    // return mp_obj_new_str((const char*)(buffer), index+1, 1);
    return mp_obj_new_bytes((const byte*)(buffer), index+1);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(M5_termout_getch_obj, M5_termout_getch);


STATIC const mp_rom_map_elem_t M5_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_m5) },

    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&M5_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawPixel), MP_ROM_PTR(&M5_TFT_drawPixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawLine), MP_ROM_PTR(&M5_TFT_drawLine_obj) },
    { MP_ROM_QSTR(MP_QSTR_fill), MP_ROM_PTR(&M5_TFT_fillScreen_obj) },
    { MP_ROM_QSTR(MP_QSTR_fillScreen), MP_ROM_PTR(&M5_TFT_fillScreen_obj) },
    { MP_ROM_QSTR(MP_QSTR_print), MP_ROM_PTR(&M5_TFT_print_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawRect), MP_ROM_PTR(&M5_TFT_drawRect_obj) },
    { MP_ROM_QSTR(MP_QSTR_fillRect), MP_ROM_PTR(&M5_TFT_fillRect_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawCircle), MP_ROM_PTR(&M5_TFT_drawCircle) },
    { MP_ROM_QSTR(MP_QSTR_fillCircle), MP_ROM_PTR(&M5_TFT_fillCircle_obj) },
    { MP_ROM_QSTR(MP_QSTR_drawTriangle), MP_ROM_PTR(&M5_TFT_drawTriangle) },
    { MP_ROM_QSTR(MP_QSTR_fillTriangle), MP_ROM_PTR(&M5_TFT_fillTriangle) },

    { MP_ROM_QSTR(MP_QSTR_termin), MP_ROM_PTR(&M5_term_stdin_obj) },
    { MP_ROM_QSTR(MP_QSTR_termout_getch), MP_ROM_PTR(&M5_termout_getch_obj) },
};
STATIC MP_DEFINE_CONST_DICT(M5_module_globals, M5_module_globals_table);

const mp_obj_module_t M5_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&M5_module_globals,
};

