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

#include <string.h>

#include "modbadge.h"

#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"

#include "bpp_init.h"

#include "badge_mpr121.h"

#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"

#define TAG "esp32/modbadge"

// INIT

STATIC mp_obj_t badge_init_() {
  badge_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_init_obj, badge_init_);

/*** nvs access ***/

static void _nvs_check_namespace(const char *namespace) {
  if (strlen(namespace) == 0 || strlen(namespace) > 15) {
    mp_raise_msg(&mp_type_AttributeError, "Invalid namespace");
  }
}

static void _nvs_check_namespace_key(const char *namespace, const char *key) {
  _nvs_check_namespace(namespace);

  if (strlen(key) == 0 || strlen(key) > 15) {
    mp_raise_msg(&mp_type_AttributeError, "Invalid key");
  }
}

STATIC mp_obj_t badge_nvs_erase_all_(mp_obj_t _namespace) {
  const char *namespace = mp_obj_str_get_str(_namespace);
  _nvs_check_namespace(namespace);

  esp_err_t err = badge_nvs_erase_all(namespace);
  if (err != ESP_OK) {
    mp_raise_msg(&mp_type_ValueError, "Failed to erase all keys in nvs");
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(badge_nvs_erase_all_obj, badge_nvs_erase_all_);

STATIC mp_obj_t badge_nvs_erase_key_(mp_obj_t _namespace, mp_obj_t _key) {
  const char *namespace = mp_obj_str_get_str(_namespace);
  const char *key       = mp_obj_str_get_str(_key);
  _nvs_check_namespace_key(namespace, key);

  esp_err_t err = badge_nvs_erase_key(namespace, key);
  if (err != ESP_OK) {
    mp_raise_msg(&mp_type_ValueError, "Failed to erase key in nvs");
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(badge_nvs_erase_key_obj, badge_nvs_erase_key_);
/* nvs: strings */
STATIC mp_obj_t badge_nvs_get_str_(mp_uint_t n_args, const mp_obj_t *args) {
  const char *namespace = mp_obj_str_get_str(args[0]);
  const char *key       = mp_obj_str_get_str(args[1]);
  _nvs_check_namespace_key(namespace, key);

  // current max string length in esp-idf is 1984 bytes, but that
  // would abuse our stack too much. we only allow strings with a
  // max length of 255 chars.
  char value[256];

  size_t length = sizeof(value);
  esp_err_t err = badge_nvs_get_str(namespace, key, value, &length);
  if (err != ESP_OK) {
    if (n_args > 2) {
      return args[2];
    }
    return mp_const_none;
  }

  return mp_obj_new_str(value, length-1, false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_nvs_get_str_obj, 2, 3, badge_nvs_get_str_);

STATIC mp_obj_t badge_nvs_set_str_(mp_obj_t _namespace, mp_obj_t _key, mp_obj_t _value) {
  const char *namespace = mp_obj_str_get_str(_namespace);
  const char *key       = mp_obj_str_get_str(_key);
  const char *value     = mp_obj_str_get_str(_value);
  _nvs_check_namespace_key(namespace, key);
  if (strlen(value) > 255) {
    mp_raise_msg(&mp_type_AttributeError, "Value string too long");
  }

  esp_err_t err = badge_nvs_set_str(namespace, key, value);
  if (err != ESP_OK) {
    mp_raise_msg(&mp_type_ValueError, "Failed to store data in nvs");
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(badge_nvs_set_str_obj, badge_nvs_set_str_);

/* nvs: u8 */
STATIC mp_obj_t badge_nvs_get_u8_(mp_uint_t n_args, const mp_obj_t *args) {
  const char *namespace = mp_obj_str_get_str(args[0]);
  const char *key       = mp_obj_str_get_str(args[1]);
  _nvs_check_namespace_key(namespace, key);

  uint8_t value = 0;
  esp_err_t err = badge_nvs_get_u8(namespace, key, &value);
  if (err != ESP_OK) {
    if (n_args > 2) {
      return args[2];
    }
    return mp_const_none;
  }

  return mp_obj_new_int(value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_nvs_get_u8_obj, 2, 3, badge_nvs_get_u8_);

STATIC mp_obj_t badge_nvs_set_u8_(mp_obj_t _namespace, mp_obj_t _key, mp_obj_t _value) {
  const char *namespace = mp_obj_str_get_str(_namespace);
  const char *key       = mp_obj_str_get_str(_key);
  int value             = mp_obj_get_int(_value);
  _nvs_check_namespace_key(namespace, key);
  if (value < 0 || value > 255) {
    mp_raise_msg(&mp_type_AttributeError, "Value out of range");
  }

  esp_err_t err = badge_nvs_set_u8(namespace, key, value);
  if (err != ESP_OK) {
    mp_raise_msg(&mp_type_ValueError, "Failed to store data in nvs");
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(badge_nvs_set_u8_obj, badge_nvs_set_u8_);

/* nvs: u16 */
STATIC mp_obj_t badge_nvs_get_u16_(mp_uint_t n_args, const mp_obj_t *args) {
  const char *namespace = mp_obj_str_get_str(args[0]);
  const char *key       = mp_obj_str_get_str(args[1]);
  _nvs_check_namespace_key(namespace, key);

  uint16_t value = 0;
  esp_err_t err = badge_nvs_get_u16(namespace, key, &value);
  if (err != ESP_OK) {
    if (n_args > 2) {
      return args[2];
    }
    return mp_const_none;
  }

  return mp_obj_new_int(value);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_nvs_get_u16_obj, 2, 3, badge_nvs_get_u16_);

STATIC mp_obj_t badge_nvs_set_u16_(mp_obj_t _namespace, mp_obj_t _key, mp_obj_t _value) {
  const char *namespace = mp_obj_str_get_str(_namespace);
  const char *key       = mp_obj_str_get_str(_key);
  int value             = mp_obj_get_int(_value);
  _nvs_check_namespace_key(namespace, key);
  if (value < 0 || value > 65535) {
    mp_raise_msg(&mp_type_AttributeError, "Value out of range");
  }

  esp_err_t err = badge_nvs_set_u16(namespace, key, value);
  if (err != ESP_OK) {
    mp_raise_msg(&mp_type_ValueError, "Failed to store data in nvs");
  }

  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(badge_nvs_set_u16_obj, badge_nvs_set_u16_);


// Mpr121 (badge_mpr121.h)
#ifdef I2C_MPR121_ADDR
#define store_dict_int(dict, field, contents) mp_obj_dict_store(dict, mp_obj_new_str(field, strlen(field), false), mp_obj_new_int(contents));
STATIC mp_obj_t badge_mpr121_get_touch_info_(void) {
	struct badge_mpr121_touch_info info;
	esp_err_t err = badge_mpr121_get_touch_info(&info);
	if (err != ESP_OK) {
		mp_raise_OSError(MP_EIO);
	}

	mp_obj_t list_items[8];
	int i;
	for (i=0; i<8; i++) {
		list_items[i] = mp_obj_new_dict(0);

		mp_obj_dict_t *dict = MP_OBJ_TO_PTR(list_items[i]);

		store_dict_int(dict, "data",     info.data[i]);
		store_dict_int(dict, "baseline", info.baseline[i]);
		store_dict_int(dict, "touch",    info.touch[i]);
		store_dict_int(dict, "release",  info.release[i]);
	}
	mp_obj_t list = mp_obj_new_list(8, list_items);
	return list;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_mpr121_get_touch_info_obj, badge_mpr121_get_touch_info_);
#endif // I2C_MPR121_ADDR


// E-Ink (badge_eink.h)

STATIC mp_obj_t badge_eink_init_() {
  badge_eink_init(BADGE_EINK_DEFAULT);
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_init_obj, badge_eink_init_);

STATIC mp_obj_t badge_eink_deep_sleep_() {
  badge_eink_deep_sleep();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_deep_sleep_obj, badge_eink_deep_sleep_);

STATIC mp_obj_t badge_eink_wakeup_() {
  badge_eink_wakeup();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_eink_wakeup_obj, badge_eink_wakeup_);


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


/* PNG READER TEST */

STATIC mp_obj_t badge_eink_png(mp_obj_t obj_x, mp_obj_t obj_y, mp_obj_t obj_filename)
{
	int x = mp_obj_get_int(obj_x);
	int y = mp_obj_get_int(obj_y);
	const char* filename = mp_obj_str_get_str(obj_filename);

	if (x >= BADGE_EINK_WIDTH || y >= BADGE_EINK_HEIGHT)
	{
		return mp_const_none;
	}

	struct lib_file_reader *fr = lib_file_new(filename, 1024);
	if (fr == NULL)
	{
		nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "Could not open file '%s'!",filename));
		return mp_const_none;
	}

	struct lib_png_reader *pr = lib_png_new((lib_reader_read_t) &lib_file_read, fr);
	if (pr == NULL)
	{
		lib_file_destroy(fr);
		nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "out of memory."));
		return mp_const_none;
	}

	uint32_t dst_min_x = x < 0 ? -x : 0;
	uint32_t dst_min_y = y < 0 ? -y : 0;
	int res = lib_png_load_image(pr, &badge_eink_fb[y * BADGE_EINK_WIDTH + x], dst_min_x, dst_min_y, BADGE_EINK_WIDTH - x, BADGE_EINK_HEIGHT - y, BADGE_EINK_WIDTH);
	lib_png_destroy(pr);
	lib_file_destroy(fr);

	if (res < 0)
	{
		nlr_raise(mp_obj_new_exception_msg_varg(&mp_type_ValueError, "failed to load image: res = %d", res));
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(badge_eink_png_obj, badge_eink_png);

/* END OF PNG READER TEST */


// Power (badge_power.h)

STATIC mp_obj_t badge_power_init_() {
  badge_power_init();
  return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_power_init_obj, badge_power_init_);

STATIC mp_obj_t battery_charge_status_() {
  return mp_obj_new_bool(badge_battery_charge_status());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(battery_charge_status_obj, battery_charge_status_);

STATIC mp_obj_t battery_volt_sense_() {
  return mp_obj_new_int(badge_battery_volt_sense());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(battery_volt_sense_obj, battery_volt_sense_);

STATIC mp_obj_t usb_volt_sense_() {
  return mp_obj_new_int(badge_usb_volt_sense());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(usb_volt_sense_obj, usb_volt_sense_);


STATIC mp_obj_t badge_power_leds_enable_() {
  return mp_obj_new_int(badge_power_leds_enable());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_power_leds_enable_obj, badge_power_leds_enable_);

STATIC mp_obj_t badge_power_leds_disable_() {
  return mp_obj_new_int(badge_power_leds_disable());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_power_leds_disable_obj, badge_power_leds_disable_);


STATIC mp_obj_t badge_power_sdcard_enable_() {
  return mp_obj_new_int(badge_power_sdcard_enable());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_power_sdcard_enable_obj, badge_power_sdcard_enable_);

STATIC mp_obj_t badge_power_sdcard_disable_() {
  return mp_obj_new_int(badge_power_sdcard_disable());
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_power_sdcard_disable_obj, badge_power_sdcard_disable_);


// LEDs

#if defined(PIN_NUM_LED) || defined(MPR121_PIN_NUM_LEDS)
STATIC mp_obj_t badge_leds_init_() {
  badge_leds_init();
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
  bool is_bytes = MP_OBJ_IS_TYPE(args[0], &mp_type_bytes);

  if (!is_bytes) {
    printf("badge.leds_send_data() used with string object instead of bytestring object.\n");
  }

  mp_uint_t len;
  uint8_t *leds = (uint8_t *)mp_obj_str_get_data(args[0], &len);
  if (n_args > 1) {
    mp_uint_t arglen = mp_obj_get_int(args[1]);
    if (len != arglen) {
      printf("badge.leds_send_data() len mismatch. (%d != %d)\n", len, arglen);
    }
  }

  return mp_obj_new_int(badge_leds_send_data(leds, len));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(badge_leds_send_data_obj, 1,2 ,badge_leds_send_data_);
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


// Mounts
static bool root_mounted = false;
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
STATIC mp_obj_t badge_mount_root() {
	// already mounted?
	if (root_mounted)
	{
		return mp_const_none;
	}

	// mount the block device
	const esp_vfs_fat_mount_config_t mount_config = {
		.max_files              = 3,
		.format_if_mount_failed = true,
	};

	ESP_LOGI(TAG, "mounting locfd on /");
	esp_err_t err = esp_vfs_fat_spiflash_mount("", "locfd", &mount_config, &s_wl_handle);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (0x%x)", err);
		mp_raise_OSError(MP_EIO);
	}

	root_mounted = true;

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_mount_root_obj, badge_mount_root);

static bool sdcard_mounted = false;
STATIC mp_obj_t badge_mount_sdcard() {
	// already mounted?
	if (sdcard_mounted)
	{
		return mp_const_none;
	}

	badge_power_sdcard_enable();

	sdmmc_host_t host = SDMMC_HOST_DEFAULT();

	// To use 1-line SD mode, uncomment the following line:
	host.flags = SDMMC_HOST_FLAG_1BIT;

	// This initializes the slot without card detect (CD) and write protect (WP) signals.
	// Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
	sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

	// Options for mounting the filesystem.
	// If format_if_mount_failed is set to true, SD card will be partitioned and formatted
	// in case when mounting fails.
	esp_vfs_fat_sdmmc_mount_config_t mount_config = {
		.max_files              = 3,
		.format_if_mount_failed = false,
	};

	// Use settings defined above to initialize SD card and mount FAT filesystem.
	// Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
	// Please check its source code and implement error recovery when developing
	// production applications.
	sdmmc_card_t* card;
	esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount filesystem. If you want the card to be formatted, set format_if_mount_failed = true.");
		} else {
			ESP_LOGE(TAG, "Failed to initialize the card (%d). Make sure SD card lines have pull-up resistors in place.", ret);
		}
		mp_raise_OSError(MP_EIO);
		return mp_const_none;
	}

	sdcard_mounted = true;

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_mount_sdcard_obj, badge_mount_sdcard);

STATIC mp_obj_t badge_unmount_sdcard() {
	// not mounted?
	if (!sdcard_mounted)
	{
		return mp_const_none;
	}

/*
	sdcard_mounted = false;

    // All done, unmount partition and disable SDMMC host peripheral
    esp_vfs_fat_sdmmc_unmount();
    ESP_LOGI(TAG, "Card unmounted");

	badge_power_sdcard_disable();
*/
	printf("Unmounting the sdcard is not yet supported.\n");

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_unmount_sdcard_obj, badge_unmount_sdcard);

static bool bpp_mounted = false;
STATIC mp_obj_t badge_mount_bpp() {
	// already mounted?
	if (bpp_mounted)
	{
		return mp_const_none;
	}

	bpp_mount_ropart();

	bpp_mounted = true;

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(badge_mount_bpp_obj, badge_mount_bpp);


// Module globals

STATIC const mp_rom_map_elem_t badge_module_globals_table[] = {
    {MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_badge)},

    {MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&badge_init_obj)},

    // Mpr121
#ifdef I2C_MPR121_ADDR
    {MP_ROM_QSTR(MP_QSTR_mpr121_get_touch_info), MP_ROM_PTR(&badge_mpr121_get_touch_info_obj)},
#endif // I2C_MPR121_ADDR

    // E-Ink
    {MP_ROM_QSTR(MP_QSTR_eink_init), MP_ROM_PTR(&badge_eink_init_obj)},
    {MP_ROM_QSTR(MP_QSTR_eink_deep_sleep), MP_ROM_PTR(&badge_eink_deep_sleep_obj)},
    {MP_ROM_QSTR(MP_QSTR_eink_wakeup), MP_ROM_PTR(&badge_eink_wakeup_obj)},

    // Power
    {MP_OBJ_NEW_QSTR(MP_QSTR_power_init), (mp_obj_t)&badge_power_init_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_battery_charge_status), (mp_obj_t)&battery_charge_status_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_battery_volt_sense), (mp_obj_t)&battery_volt_sense_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_usb_volt_sense), (mp_obj_t)&usb_volt_sense_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_power_leds_enable), (mp_obj_t)&badge_power_leds_enable_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_power_leds_disable), (mp_obj_t)&badge_power_leds_disable_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_power_sdcard_enable), (mp_obj_t)&badge_power_sdcard_enable_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_power_sdcard_disable), (mp_obj_t)&badge_power_sdcard_disable_obj},

    // NVS
    {MP_ROM_QSTR(MP_QSTR_nvs_erase_all), MP_ROM_PTR(&badge_nvs_erase_all_obj)},
    {MP_ROM_QSTR(MP_QSTR_nvs_erase_key), MP_ROM_PTR(&badge_nvs_erase_key_obj)},
    {MP_ROM_QSTR(MP_QSTR_nvs_get_str), MP_ROM_PTR(&badge_nvs_get_str_obj)},
    {MP_ROM_QSTR(MP_QSTR_nvs_set_str), MP_ROM_PTR(&badge_nvs_set_str_obj)},
    {MP_ROM_QSTR(MP_QSTR_nvs_get_u8), MP_ROM_PTR(&badge_nvs_get_u8_obj)},
    {MP_ROM_QSTR(MP_QSTR_nvs_set_u8), MP_ROM_PTR(&badge_nvs_set_u8_obj)},
    {MP_ROM_QSTR(MP_QSTR_nvs_get_u16), MP_ROM_PTR(&badge_nvs_get_u16_obj)},
    {MP_ROM_QSTR(MP_QSTR_nvs_set_u16), MP_ROM_PTR(&badge_nvs_set_u16_obj)},

#if defined(PIN_NUM_LED) || defined(MPR121_PIN_NUM_LEDS)
    // LEDs
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_init), (mp_obj_t)&badge_leds_init_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_enable), (mp_obj_t)&badge_leds_enable_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_disable), (mp_obj_t)&badge_leds_disable_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_leds_send_data), (mp_obj_t)&badge_leds_send_data_obj},
#endif

#if defined(PORTEXP_PIN_NUM_VIBRATOR) || defined(MPR121_PIN_NUM_VIBRATOR)
    {MP_OBJ_NEW_QSTR(MP_QSTR_vibrator_init), (mp_obj_t)&badge_vibrator_init_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_vibrator_activate), (mp_obj_t)&badge_vibrator_activate_obj},
#endif

    {MP_ROM_QSTR(MP_QSTR_eink_busy), MP_ROM_PTR(&badge_eink_busy_obj)},
    {MP_ROM_QSTR(MP_QSTR_eink_busy_wait), MP_ROM_PTR(&badge_eink_busy_wait_obj)},

    {MP_ROM_QSTR(MP_QSTR_eink_png), MP_ROM_PTR(&badge_eink_png_obj)},

/*
    {MP_ROM_QSTR(MP_QSTR_display_picture), MP_ROM_PTR(&badge_display_picture_obj)},
*/

	// Mounts
    {MP_OBJ_NEW_QSTR(MP_QSTR_mount_root), (mp_obj_t)&badge_mount_root_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_mount_sdcard), (mp_obj_t)&badge_mount_sdcard_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_unmount_sdcard), (mp_obj_t)&badge_unmount_sdcard_obj},
    {MP_OBJ_NEW_QSTR(MP_QSTR_mount_bpp), (mp_obj_t)&badge_mount_bpp_obj},

};

STATIC MP_DEFINE_CONST_DICT(badge_module_globals, badge_module_globals_table);

const mp_obj_module_t badge_module = {
    .base = {&mp_type_module},
    .globals = (mp_obj_dict_t *)&badge_module_globals,
};
