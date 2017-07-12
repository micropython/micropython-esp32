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

#include "esp_spi_flash.h"
#include "wear_levelling.h"

#include "py/runtime.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "drivers/dht/dht.h"
#include "modesp.h"

#if MICROPY_SDMMC_USE_DRIVER
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "sdmmc_cmd.h"
#endif


STATIC wl_handle_t fs_handle = WL_INVALID_HANDLE;   // not initialized
STATIC size_t wl_sect_size = 4096;                  // will be set to actual size after initialization

// esp32 partition configuration needed for wear leveling driver
STATIC const esp_partition_t fs_part = {
    ESP_PARTITION_TYPE_DATA,        //type
    ESP_PARTITION_SUBTYPE_DATA_FAT, //subtype
    MICROPY_INTERNALFS_START,       // address (from mpconfigport.h)
    MICROPY_INTERNALFS_SIZE,        // size (from mpconfigport.h)
    "uPYpart",                      // label
    MICROPY_INTERNALFS_ENCRIPTED    // encrypted (from mpconfigport.h)
};


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
    // on first call to this function wear leveling partition is not yet initialized!
    if (fs_handle == WL_INVALID_HANDLE) {
        // if wear leveling partition is not 'mounted', do it now
        esp_err_t res = wl_mount(&fs_part, &fs_handle);
        if (res != ESP_OK) {
            // return the size less than 1MB to indicate an error
            return mp_obj_new_int_from_uint(0x010000);
        }
        wl_sect_size = wl_sector_size(fs_handle);
    }
    // return usable file system size in bytes
    return mp_obj_new_int_from_uint(wl_size(fs_handle));
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_flash_size_obj, esp_flash_size);

STATIC mp_obj_t esp_flash_sec_size() {
    return mp_obj_new_int_from_uint(wl_sect_size);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_flash_sec_size_obj, esp_flash_sec_size);

STATIC IRAM_ATTR mp_obj_t esp_flash_user_start(void) {
    // wL driver starts from address 0, which is mapped to configured physical Flash address
    return MP_OBJ_NEW_SMALL_INT(0);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_flash_user_start_obj, esp_flash_user_start);

#if MICROPY_SDMMC_USE_DRIVER

// ======== SD Card support ===========================================================================

/*
 * ##### Using SDCard with sdmmc driver connection: ###################################################

1-bit	ESP32 pin     | SDc SD     uSD | Notes
----------------------|----------------|------------
SCLK	GPIO14 (MTMS) | CLK  5      5  | 10k pullup
MOSI	GPIO15 (MTDO) | CMD  2      3  | 10k pullup
MISO	GPIO2         | D0   7      7  | 10k pullup, pull low to go into download mode
		GPIO4         | D1   8      8  | 10k pullup; not used in 1-line mode
		GPIO12 (MTDI) | D2   9      1  | otherwise 10k pullup (see note below!); not used in 1-line mode
CS		GPIO13 (MTCK) | D3   1      2  | 10k pullup needed at card side, even in 1-line mode
VDD     3.3V          | VSS  4      4  |
GND     GND           | GND  3&6    6  |
		N/C           | CD             |
		N/C           | WP             |

SDcard pinout                 uSDcard pinout
                 Contacts view
 _________________             1 2 3 4 5 6 7 8
|                 |            _______________
|                 |           |# # # # # # # #|
|                 |           |               |
|                 |           |               |
|                 |           /               |
|                 |          /                |
|                 |         |_                |
|                 |           |               |
|                #|          /                |
|# # # # # # # # /          |                 |
|_______________/           |                 |
 8 7 6 5 4 3 2 1 9          |_________________|

 * ####################################################################################################
*/


STATIC sdmmc_card_t sdmmc_card;
STATIC uint8_t sdcard_status = 1;

//---------------------------------------------------------------
STATIC void sdcard_print_info(const sdmmc_card_t* card, int mode)
{
    #if MICROPY_SDMMC_SHOW_INFO
	printf("---------------------\n");
	if (mode == 1) {
        printf(" Mode: 1-line mode\n");
    } else {
        printf(" Mode:  SD (4bit)\n");
    }
    printf(" Name: %s\n", card->cid.name);
    printf(" Type: %s\n", (card->ocr & SD_OCR_SDHC_CAP)?"SDHC/SDXC":"SDSC");
    printf("Speed: %s (%d MHz)\n", (card->csd.tr_speed > 25000000)?"high speed":"default speed", card->csd.tr_speed/1000000);
    printf(" Size: %u MB\n", (uint32_t)(((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024)));
    printf("  CSD: ver=%d, sector_size=%d, capacity=%d read_bl_len=%d\n",
            card->csd.csd_ver,
            card->csd.sector_size, card->csd.capacity, card->csd.read_block_len);
    printf("  SCR: sd_spec=%d, bus_width=%d\n\n", card->scr.sd_spec, card->scr.bus_width);
    #endif
}

//----------------------------------------------
STATIC mp_obj_t esp_sdcard_init(mp_obj_t mode) {
    mp_int_t card_mode = mp_obj_get_int(mode);

    if (sdcard_status == 0) {
        #if MICROPY_SDMMC_SHOW_INFO
        printf("Already initialized:\n");
        sdcard_print_info(&sdmmc_card, card_mode);
        #endif
        return MP_OBJ_NEW_SMALL_INT(sdcard_status);
    }

    // Configure sdmmc interface
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();

    // Enable pull-ups on the SD card pins
    // ** It is recommended to use external 10K pull-ups **
    gpio_set_pull_mode(2, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(14, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(15, GPIO_PULLUP_ONLY);

	if (card_mode == 1) {
        // Use 1-line SD mode
        host.flags = SDMMC_HOST_FLAG_1BIT;
        slot_config.width = 1;
    }
    else {
        gpio_set_pull_mode(4, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(12, GPIO_PULLUP_ONLY);
        gpio_set_pull_mode(13, GPIO_PULLUP_ONLY);
    }

    sdmmc_host_init();
    sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);

    // Initialize the sd card
    esp_log_level_set("*", ESP_LOG_NONE);
    #if MICROPY_SDMMC_SHOW_INFO
	printf("---------------------\n");
    printf("Initializing SD Card: ");
    #endif
    esp_err_t res = sdmmc_card_init(&host, &sdmmc_card);
	esp_log_level_set("*", ESP_LOG_ERROR);

    if (res == ESP_OK) {
        sdcard_status = ESP_OK;
        #if MICROPY_SDMMC_SHOW_INFO
        printf("OK.\n");
        #endif
        sdcard_print_info(&sdmmc_card, card_mode);
    } else {
        sdcard_status = 1;
        #if MICROPY_SDMMC_SHOW_INFO
        printf("Error.\n\n");
        #endif
    }
    return MP_OBJ_NEW_SMALL_INT(sdcard_status);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(esp_sdcard_init_obj, esp_sdcard_init);

//-------------------------------------------------------------------------
STATIC mp_obj_t esp_sdcard_read(mp_obj_t ulSectorNumber, mp_obj_t buf_in) {
    mp_int_t sect_num = mp_obj_get_int(ulSectorNumber);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_WRITE);
    mp_int_t sect_count = bufinfo.len / sdmmc_card.csd.sector_size;
    mp_int_t sect_err = bufinfo.len % sdmmc_card.csd.sector_size;

    if (sdcard_status != ESP_OK) {
        mp_raise_OSError(MP_EIO);
    }
    if (sect_count == 0) {
        mp_raise_OSError(MP_EIO);
    }
    if (sect_err) {
        mp_raise_OSError(MP_EIO);
    }

    esp_err_t res = sdmmc_read_sectors(&sdmmc_card, bufinfo.buf, sect_num, sect_count);
    if (res != ESP_OK) {
        mp_raise_OSError(MP_EIO);
    }

    //printf("[SD] read sect=%d, count=%d, size=%d\n", sect_num, sect_count, bufinfo.len);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(esp_sdcard_read_obj, esp_sdcard_read);


//--------------------------------------------------------------------------
STATIC mp_obj_t esp_sdcard_write(mp_obj_t ulSectorNumber, mp_obj_t buf_in) {
    mp_int_t sect_num = mp_obj_get_int(ulSectorNumber);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);
    mp_int_t sect_count = bufinfo.len / sdmmc_card.csd.sector_size;
    mp_int_t sect_err = bufinfo.len % sdmmc_card.csd.sector_size;

    if (sdcard_status != ESP_OK) {
        mp_raise_OSError(MP_EIO);
    }
    if (sect_count == 0) {
        mp_raise_OSError(MP_EIO);
    }
    if (sect_err) {
        mp_raise_OSError(MP_EIO);
    }

    int res = sdmmc_write_sectors(&sdmmc_card, bufinfo.buf, sect_num, sect_count);
    if (res != ESP_OK) {
        mp_raise_OSError(MP_EIO);
    }

    //printf("[SD] write sect=%d, count=%d, size=%d\n", sect_num, sect_count, bufinfo.len);
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(esp_sdcard_write_obj, esp_sdcard_write);


//-------------------------------------------
STATIC mp_obj_t esp_sdcard_sect_count(void) {
    if (sdcard_status == ESP_OK) {
        return MP_OBJ_NEW_SMALL_INT(sdmmc_card.csd.capacity);
    }
    else {
        return MP_OBJ_NEW_SMALL_INT(0);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_sdcard_sect_count_obj, esp_sdcard_sect_count);

//------------------------------------------
STATIC mp_obj_t esp_sdcard_sect_size(void) {
    if (sdcard_status == ESP_OK) {
        return MP_OBJ_NEW_SMALL_INT(sdmmc_card.csd.sector_size);
    }
    else {
        return MP_OBJ_NEW_SMALL_INT(0);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(esp_sdcard_sect_size_obj, esp_sdcard_sect_size);


// ======== ^^^^^^^^^^^^^^^ ===========================================================================

#endif  // MICROPY_SDMMC_USE_DRIVER


STATIC mp_obj_t esp_neopixel_write_(mp_obj_t pin, mp_obj_t buf, mp_obj_t timing) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf, &bufinfo, MP_BUFFER_READ);
    esp_neopixel_write(mp_hal_get_pin_obj(pin),
        (uint8_t*)bufinfo.buf, bufinfo.len, mp_obj_get_int(timing));
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(esp_neopixel_write_obj, esp_neopixel_write_);

STATIC const mp_rom_map_elem_t esp_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_esp) },

    { MP_ROM_QSTR(MP_QSTR_flash_read), MP_ROM_PTR(&esp_flash_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_write), MP_ROM_PTR(&esp_flash_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_erase), MP_ROM_PTR(&esp_flash_erase_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_size), MP_ROM_PTR(&esp_flash_size_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_user_start), MP_ROM_PTR(&esp_flash_user_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_flash_sec_size), MP_ROM_PTR(&esp_flash_sec_size_obj) },

    #if MICROPY_SDMMC_USE_DRIVER
    { MP_ROM_QSTR(MP_QSTR_sdcard_read), MP_ROM_PTR(&esp_sdcard_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_sdcard_write), MP_ROM_PTR(&esp_sdcard_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_sdcard_init), MP_ROM_PTR(&esp_sdcard_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_sdcard_sect_count), MP_ROM_PTR(&esp_sdcard_sect_count_obj) },
    { MP_ROM_QSTR(MP_QSTR_sdcard_sect_size), MP_ROM_PTR(&esp_sdcard_sect_size_obj) },
    // class constants
    { MP_ROM_QSTR(MP_QSTR_SD_1LINE), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_SD_4LINE), MP_ROM_INT(4) },
    #endif

    { MP_ROM_QSTR(MP_QSTR_neopixel_write), MP_ROM_PTR(&esp_neopixel_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_dht_readinto), MP_ROM_PTR(&dht_readinto_obj) },
};

STATIC MP_DEFINE_CONST_DICT(esp_module_globals, esp_module_globals_table);

const mp_obj_module_t esp_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t*)&esp_module_globals,
};

