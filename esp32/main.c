/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task.h"

#include "py/stackctrl.h"
#include "py/nlr.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "lib/mp-readline/readline.h"
#include "lib/utils/pyexec.h"
#include "uart.h"

// MicroPython runs as a task under FreeRTOS
#define MP_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)
#define MP_TASK_STACK_SIZE      (16 * 1024)
#define MP_TASK_STACK_LEN       (MP_TASK_STACK_SIZE / sizeof(StackType_t))
#define MP_TASK_HEAP_SIZE       (64 * 1024)

//STATIC StaticTask_t mp_task_tcb;
//STATIC StackType_t mp_task_stack[MP_TASK_STACK_LEN] __attribute__((aligned (8)));
STATIC uint8_t mp_task_heap[MP_TASK_HEAP_SIZE];

void mp_task(void *pvParameter) {
    uart_init();
soft_reset:
    mp_stack_set_top((void*)&pvParameter);
    mp_stack_set_limit(MP_TASK_STACK_SIZE - 512);
    gc_init(mp_task_heap, mp_task_heap + sizeof(mp_task_heap));
    mp_init();
    MP_STATE_PORT(mp_kbd_exception) = mp_obj_new_exception(&mp_type_KeyboardInterrupt);
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_init(mp_sys_argv, 0);
    readline_init0();

    // run boot-up scripts
    pyexec_frozen_module("_boot.py");
    pyexec_file("boot.py");
    if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
        pyexec_file("main.py");
    }

    for (;;) {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
            if (pyexec_raw_repl() != 0) {
                break;
            }
        } else {
            if (pyexec_friendly_repl() != 0) {
                break;
            }
        }
    }

    mp_hal_stdout_tx_str("PYB: soft reboot\r\n");
    mp_deinit();
    fflush(stdout);
    goto soft_reset;
}

void app_main(void) {
    nvs_flash_init();
    // TODO use xTaskCreateStatic (needs custom FreeRTOSConfig.h)
    xTaskCreatePinnedToCore(mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, NULL, 0);
    //xTaskCreateStatic(mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY, &mp_task_stack[0], &mp_task_tcb);
}

void nlr_jump_fail(void *val) {
    printf("NLR jump failed, val=%p\n", val);
    for (;;) {
    }
}

mp_import_stat_t fat_vfs_import_stat(const char *path);

mp_import_stat_t mp_import_stat(const char *path) {
    #if MICROPY_VFS_FAT
    return fat_vfs_import_stat(path);
    #else
    (void)path;
    return MP_IMPORT_STAT_NO_EXIST;
    #endif
}

mp_obj_t vfs_proxy_call(qstr method_name, size_t n_args, const mp_obj_t *args);
mp_obj_t mp_builtin_open(uint n_args, const mp_obj_t *args, mp_map_t *kwargs) {
    #if MICROPY_VFS_FAT
    // TODO: Handle kwargs!
    return vfs_proxy_call(MP_QSTR_open, n_args, args);
    #else
    return mp_const_none;
    #endif
}
MP_DEFINE_CONST_FUN_OBJ_KW(mp_builtin_open_obj, 1, mp_builtin_open);

// modussl_mbedtls uses this function but it's not enabled in ESP IDF
void mbedtls_debug_set_threshold(int threshold) {
    (void)threshold;
}
