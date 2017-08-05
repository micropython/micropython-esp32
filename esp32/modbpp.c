#include <stdio.h>
#include <string.h>
#include "esp_spi_flash.h"
#include "wear_levelling.h"

#include "drivers/dht/dht.h"
#include "modesp.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"

STATIC wl_handle_t fs_handle = WL_INVALID_HANDLE;
STATIC size_t wl_sect_size = 4096;

STATIC const esp_partition_t *fs_part = NULL;

static void bpp_flash_init(void) {
	if (fs_handle != WL_INVALID_HANDLE)
		return;

	if (fs_part == NULL) {
		fs_part = esp_partition_find_first(0x20, 0x17, "remfs");
		if (fs_part == NULL) {
			printf("Bpp partition not found.\n");
			mp_raise_OSError(MP_EIO);
		}
	}

	esp_err_t res = wl_mount(fs_part, &fs_handle);
	if (res != ESP_OK) {
		printf("Failed to initialize wear-leveling on bpp partition.\n");
		mp_raise_OSError(MP_EIO);
	}

	wl_sect_size = wl_sector_size(fs_handle);
}

STATIC mp_obj_t bpp_flash_read(mp_obj_t offset_in, mp_obj_t buf_in) {
	bpp_flash_init();
	mp_int_t offset = mp_obj_get_int(offset_in);
	mp_buffer_info_t bufinfo;
	mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_WRITE);

	esp_err_t res = wl_read(fs_handle, offset, bufinfo.buf, bufinfo.len);
	if (res != ESP_OK) {
		mp_raise_OSError(MP_EIO);
	}
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bpp_flash_read_obj, bpp_flash_read);

STATIC mp_obj_t bpp_flash_write(mp_obj_t offset_in, mp_obj_t buf_in) {
	bpp_flash_init();
	mp_int_t offset = mp_obj_get_int(offset_in);
	mp_buffer_info_t bufinfo;
	mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);

	esp_err_t res = wl_write(fs_handle, offset, bufinfo.buf, bufinfo.len);
	if (res != ESP_OK) {
		mp_raise_OSError(MP_EIO);
	}
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(bpp_flash_write_obj, bpp_flash_write);

STATIC mp_obj_t bpp_flash_erase(mp_obj_t sector_in) {
	bpp_flash_init();
	mp_int_t sector = mp_obj_get_int(sector_in);

	esp_err_t res = wl_erase_range(fs_handle, sector * wl_sect_size, wl_sect_size);
	if (res != ESP_OK) {
		mp_raise_OSError(MP_EIO);
	}
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(bpp_flash_erase_obj, bpp_flash_erase);

STATIC mp_obj_t bpp_flash_size(void) {
	bpp_flash_init();
	return mp_obj_new_int_from_uint(fs_part->size);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(bpp_flash_size_obj, bpp_flash_size);

STATIC mp_obj_t bpp_flash_sec_size() {
	bpp_flash_init();
	return mp_obj_new_int_from_uint(wl_sect_size);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(bpp_flash_sec_size_obj, bpp_flash_sec_size);


STATIC const mp_rom_map_elem_t bpp_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_bpp)},

    { MP_ROM_QSTR(MP_QSTR_flash_read), MP_ROM_PTR(&bpp_flash_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_write), MP_ROM_PTR(&bpp_flash_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_erase), MP_ROM_PTR(&bpp_flash_erase_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_size), MP_ROM_PTR(&bpp_flash_size_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_sec_size), MP_ROM_PTR(&bpp_flash_sec_size_obj) },
};

STATIC MP_DEFINE_CONST_DICT(bpp_module_globals, bpp_module_globals_table);

const mp_obj_module_t bpp_module = {
    .base = {&mp_type_module}, .globals = (mp_obj_dict_t *)&bpp_module_globals,
};
