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
#include <string.h>
#include "esp_spi_flash.h"
#include "wear_levelling.h"

#include "drivers/dht/dht.h"
#include "modesp.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"
#include "rom/rtc.h"

STATIC wl_handle_t fs_handle = WL_INVALID_HANDLE;
STATIC size_t wl_sect_size = 4096;

STATIC const esp_partition_t fs_part;

STATIC mp_obj_t esp_flash_read(mp_obj_t offset_in, mp_obj_t buf_in) {
    mp_int_t offset = mp_obj_get_int(offset_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_WRITE);

    esp_err_t res = wl_read(fs_handle, offset, bufinfo.buf, bufinfo.len);
    if (res != ESP_OK) {
        mp_raise_OSError(MP_EIO);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(esp_flash_read_obj, esp_flash_read);

STATIC mp_obj_t esp_flash_write(mp_obj_t offset_in, mp_obj_t buf_in) {
    mp_int_t offset = mp_obj_get_int(offset_in);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);

    esp_err_t res = wl_write(fs_handle, offset, bufinfo.buf, bufinfo.len);
    if (res != ESP_OK) {
        mp_raise_OSError(MP_EIO);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(esp_flash_write_obj, esp_flash_write);

STATIC mp_obj_t esp_flash_erase(mp_obj_t sector_in) {
    mp_int_t sector = mp_obj_get_int(sector_in);

    esp_err_t res = wl_erase_range(fs_handle, sector * wl_sect_size, wl_sect_size);
    if (res != ESP_OK) {
        mp_raise_OSError(MP_EIO);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_flash_erase_obj, esp_flash_erase);

STATIC mp_obj_t esp_flash_size(void) {
  if (fs_handle == WL_INVALID_HANDLE) {
      esp_partition_t *part
          = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "locfd");
      if (part == NULL) {
          return mp_obj_new_int_from_uint(0);
      }
      memcpy(&fs_part, part, sizeof(esp_partition_t));

      esp_err_t res = wl_mount(&fs_part, &fs_handle);
      if (res != ESP_OK) {
          return mp_obj_new_int_from_uint(0);
      }
      wl_sect_size = wl_sector_size(fs_handle);
  }
  return mp_obj_new_int_from_uint(fs_part.size);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_flash_size_obj, esp_flash_size);

STATIC mp_obj_t esp_flash_sec_size() {
  return mp_obj_new_int_from_uint(wl_sect_size);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_flash_sec_size_obj, esp_flash_sec_size);

STATIC IRAM_ATTR mp_obj_t esp_flash_user_start(void) {
  return MP_OBJ_NEW_SMALL_INT(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_flash_user_start_obj, esp_flash_user_start);

STATIC mp_obj_t esp_rtcmem_write_(mp_obj_t pos, mp_obj_t val) {
  esp_rtcmem_write(mp_obj_get_int(pos), mp_obj_get_int(val));
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(esp_rtcmem_write_obj, esp_rtcmem_write_);

STATIC mp_obj_t esp_rtcmem_read_(mp_obj_t pos) {
  uint8_t val = esp_rtcmem_read(mp_obj_get_int(pos));
  return mp_obj_new_int(val);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_rtcmem_read_obj, esp_rtcmem_read_);


STATIC mp_obj_t esp_rtcmem_read_string_(mp_uint_t n_args, const mp_obj_t *args) {
  int pos = 2;
  if (n_args > 0){
    pos = mp_obj_get_int(args[0]);
	}
  char words[USER_RTC_MEM_SIZE];
  esp_rtcmem_read_string(pos, words);
  return mp_obj_new_str(words, strlen(words), true);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp_rtcmem_read_string_obj, 0, 1, esp_rtcmem_read_string_);

STATIC mp_obj_t esp_rtcmem_write_string_(mp_uint_t n_args, const mp_obj_t *args) {
  int pos = 2;
  if (n_args > 1){
    pos = mp_obj_get_int(args[1]);
	}
  mp_uint_t len;
  esp_rtcmem_write_string(pos, mp_obj_str_get_data(args[0], &len));
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(esp_rtcmem_write_string_obj, 1, 2, esp_rtcmem_write_string_);


STATIC mp_obj_t esp_rtc_get_reset_reason_(mp_obj_t cpu) {
  uint8_t cpu_id = mp_obj_get_int(cpu);
  if (cpu_id > 1) {
    return mp_obj_new_int(0);
  }
  uint8_t val = rtc_get_reset_reason(cpu_id);
  return mp_obj_new_int(val);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_rtc_get_reset_reason_obj,
                                 esp_rtc_get_reset_reason_);

STATIC mp_obj_t esp_start_sleeping_(mp_obj_t time) {
  esp_start_sleeping(mp_obj_get_int(time));
  return mp_const_none;
}

STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_start_sleeping_obj, esp_start_sleeping_);

STATIC const mp_rom_map_elem_t esp_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_esp)},

    { MP_ROM_QSTR(MP_QSTR_flash_read), MP_ROM_PTR(&esp_flash_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_write), MP_ROM_PTR(&esp_flash_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_erase), MP_ROM_PTR(&esp_flash_erase_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_size), MP_ROM_PTR(&esp_flash_size_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_user_start), MP_ROM_PTR(&esp_flash_user_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_sec_size), MP_ROM_PTR(&esp_flash_sec_size_obj) },

    {MP_ROM_QSTR(MP_QSTR_rtcmem_write), MP_ROM_PTR(&esp_rtcmem_write_obj)},
    {MP_ROM_QSTR(MP_QSTR_rtcmem_read), MP_ROM_PTR(&esp_rtcmem_read_obj)},
    {MP_ROM_QSTR(MP_QSTR_rtcmem_write_string), MP_ROM_PTR(&esp_rtcmem_write_string_obj)},
    {MP_ROM_QSTR(MP_QSTR_rtcmem_read_string), MP_ROM_PTR(&esp_rtcmem_read_string_obj)},
    {MP_ROM_QSTR(MP_QSTR_rtc_get_reset_reason),
     MP_ROM_PTR(&esp_rtc_get_reset_reason_obj)},

    {MP_ROM_QSTR(MP_QSTR_start_sleeping), MP_ROM_PTR(&esp_start_sleeping_obj)},

    {MP_ROM_QSTR(MP_QSTR_dht_readinto), MP_ROM_PTR(&dht_readinto_obj)},
};

STATIC MP_DEFINE_CONST_DICT(esp_module_globals, esp_module_globals_table);

const mp_obj_module_t esp_module = {
    .base = {&mp_type_module}, .globals = (mp_obj_dict_t *)&esp_module_globals,
};
