/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
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

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_task.h"
#include "soc/cpu.h"

#include "sha2017_ota.h"
#include "esprtcmem.h"

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
#include "modmachine.h"
#include "mpthreadport.h"
#include "bpp_init.h"
#include "badge_pins.h"
#include "badge_base.h"
#include "badge_first_run.h"
#include <badge_input.h>
#include <badge.h>
#include "powerdown.h"

// MicroPython runs as a task under FreeRTOS
#define MP_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)
#define MP_TASK_STACK_SIZE      ( 8 * 1024)
#define MP_TASK_STACK_LEN       (MP_TASK_STACK_SIZE / sizeof(StackType_t))
#define MP_TASK_HEAP_SIZE       (88 * 1024)

STATIC StaticTask_t mp_task_tcb;
STATIC StackType_t mp_task_stack[MP_TASK_STACK_LEN] __attribute__((aligned (8)));
STATIC uint8_t mp_task_heap[MP_TASK_HEAP_SIZE];

extern uint32_t reset_cause;

void mp_task(void *pvParameter) {
    volatile uint32_t sp = (uint32_t)get_sp();
    #if MICROPY_PY_THREAD
    mp_thread_init(&mp_task_stack[0], MP_TASK_STACK_LEN);
    #endif
    uart_init();
    machine_init();

soft_reset:
    // initialise the stack pointer for the main thread
    mp_stack_set_top((void *)sp);
    mp_stack_set_limit(MP_TASK_STACK_SIZE - 1024);
    gc_init(mp_task_heap, mp_task_heap + sizeof(mp_task_heap));
    mp_init();
    mp_obj_list_init(mp_sys_path, 0);
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR_));
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));
    mp_obj_list_init(mp_sys_argv, 0);
    readline_init0();

    // initialise peripherals
    machine_pins_init();

    // run boot-up scripts
    pyexec_frozen_module("_boot.py");
    pyexec_file("boot.py");
    // if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL) {
    //     pyexec_file("main.py");
    // }

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

    #if MICROPY_PY_THREAD
    mp_thread_deinit();
    #endif

    mp_hal_stdout_tx_str("SHA2017Badge: soft reboot\r\n");

    // deinitialise peripherals
    machine_pins_deinit();

    mp_deinit();
    fflush(stdout);
    reset_cause = MACHINE_SOFT_RESET;
    goto soft_reset;
}

void do_bpp_bgnd() {
    // Kick off bpp
    bpp_init();

    printf("Bpp inited.\n");

    // immediately abort and reboot when touchpad detects something
    while (badge_input_get_event(1000) == 0) { }

    printf("Touch detected. Exiting bpp, rebooting.\n");
    esp_restart();
}

//PowerDownManager callback. If all objects that call into the powerdown manager are OK to sleep,
//it will calculate the time it can sleep and the mode to wake up in. 
void do_deep_sleep(int delayMs, void *arg, PowerMode mode) {
	PowerMode currMode=(PowerMode)arg;
	delayMs-=8000; //to compensate for startup delay
	if (delayMs<0) delayMs=0;
	if (currMode==mode && delayMs<5000) return; //not worth sleeping

	printf("Sleeping for %d ms...\n", delayMs);

	//Shutdown anything running
	if (currMode==POWER_MODE_BPP) {
		bpp_shutdown();
	}

	//Select wake mode
	uint8_t rtcmodebit=0;
	if (mode==POWER_MODE_BPP) rtcmodebit=2;
	if (mode==POWER_MODE_UPY) rtcmodebit=0;
	esp_rtcmem_write(0, rtcmodebit);
	esp_rtcmem_write(1, ~rtcmodebit);

	// TODO the wake on touch should be in badge_input_init
	esp_deep_sleep_enable_ext0_wakeup(GPIO_NUM_25 , 0);
	// FIXME don't use hardcoded GPIO_NUM_25
	esp_deep_sleep_enable_timer_wakeup(delayMs*1000);
	esp_deep_sleep_start();
}


void app_main(void) {
	badge_check_first_run();
	badge_base_init();

	uint8_t magic = esp_rtcmem_read(0);
	uint8_t inv_magic = esp_rtcmem_read(1);

	if (magic == (uint8_t)~inv_magic) {
		printf("Magic checked out!\n");
		switch (magic) {
			case 1:
				printf("Starting OTA\n");
				sha2017_ota_update();
				break;

#ifdef CONFIG_SHA_BPP_ENABLE
			case 2:
				badge_init();
				if (badge_input_button_state == 0) {
					printf("Starting bpp.\n");
					powerDownMgrInit(do_deep_sleep, (void*)POWER_MODE_BPP, POWER_MODE_BPP, true);
					do_bpp_bgnd();
				} else {
					printf("Touch wake after bpp.\n");
					powerDownMgrInit(do_deep_sleep, (void*)POWER_MODE_UPY, POWER_MODE_UPY, false);
					xTaskCreateStaticPinnedToCore(mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY,
					&mp_task_stack[0], &mp_task_tcb, 0);
				}
				break;
#endif
			case 3:
				badge_first_run();
		}

	} else {
		powerDownMgrInit(do_deep_sleep, (void*)POWER_MODE_UPY, POWER_MODE_UPY, false);
		xTaskCreateStaticPinnedToCore(mp_task, "mp_task", MP_TASK_STACK_LEN, NULL, MP_TASK_PRIORITY,
				&mp_task_stack[0], &mp_task_tcb, 0);
	}
}

void nlr_jump_fail(void *val) {
    printf("NLR jump failed, val=%p\n", val);
    esp_restart();
}

// modussl_mbedtls uses this function but it's not enabled in ESP IDF
void mbedtls_debug_set_threshold(int threshold) {
    (void)threshold;
}
