/*
 * Copyright (c) 2016, Pycom Limited.
 *
 * This software is licensed under the GNU GPL version 3 or any
 * later version, with permitted additional terms. For more information
 * see the Pycom Licence v1.0 document supplied with this file, or
 * available at https://www.pycom.io/opensource/licensing
 */

#include <stdint.h>
#include <string.h>

#include "py/mpstate.h"
#include "py/runtime.h"
#include "py/mphal.h"

#include "sdkconfig.h"
#include "rom/rtc.h"
#include "esp_system.h"
#include "esp_deep_sleep.h"
#include "mpsleep.h"

/******************************************************************************
 DECLARE PRIVATE DATA
 ******************************************************************************/
STATIC mpsleep_reset_cause_t mpsleep_reset_cause = MPSLEEP_PWRON_RESET;
STATIC mpsleep_wake_reason_t mpsleep_wake_reason = MPSLEEP_PWRON_WAKE;

/******************************************************************************
 DEFINE PUBLIC FUNCTIONS
 ******************************************************************************/
void mpsleep_init0 (void) {
    // check the reset casue (if it's soft reset, leave it as it is)
    switch (rtc_get_reset_reason(0)) {
        case TG0WDT_SYS_RESET:
            mpsleep_reset_cause = MPSLEEP_WDT_RESET;
            break;
        case DEEPSLEEP_RESET:
            mpsleep_reset_cause = MPSLEEP_DEEPSLEEP_RESET;
            break;
        case RTCWDT_BROWN_OUT_RESET:
            mpsleep_reset_cause = MPSLEEP_BROWN_OUT_RESET;
            break;
        case TG1WDT_SYS_RESET:      // machine.reset()
            mpsleep_reset_cause = MPSLEEP_HARD_RESET;
            break;
        case POWERON_RESET:
        case RTCWDT_RTC_RESET:      // silicon bug after power on
        default:
            mpsleep_reset_cause = MPSLEEP_PWRON_RESET;
            break;
    }

    // check the wakeup reason
    switch (esp_deep_sleep_get_wakeup_cause()) {
        case ESP_DEEP_SLEEP_WAKEUP_EXT0:
        case ESP_DEEP_SLEEP_WAKEUP_EXT1:
            mpsleep_wake_reason = MPSLEEP_GPIO_WAKE;
            break;
        case ESP_DEEP_SLEEP_WAKEUP_TIMER:
            mpsleep_wake_reason = MPSLEEP_RTC_WAKE;
            break;
        case ESP_DEEP_SLEEP_WAKEUP_UNDEFINED:
        default:
            mpsleep_wake_reason = MPSLEEP_PWRON_WAKE;
            break;
    }
}

void mpsleep_signal_soft_reset (void) {
    mpsleep_reset_cause = MPSLEEP_SOFT_RESET;
}

mpsleep_reset_cause_t mpsleep_get_reset_cause (void) {
    return mpsleep_reset_cause;
}

mpsleep_wake_reason_t mpsleep_get_wake_reason (void) {
    return mpsleep_wake_reason;
}

void mpsleep_get_reset_desc (char *reset_reason) {
    switch (mpsleep_reset_cause) {
        case MPSLEEP_PWRON_RESET:
            sprintf(reset_reason, "Power on reset");
            break;
        case MPSLEEP_HARD_RESET:
            sprintf(reset_reason, "Hard reset");
            break;
        case MPSLEEP_WDT_RESET:
            sprintf(reset_reason, "WDT reset");
            break;
        case MPSLEEP_DEEPSLEEP_RESET:
            sprintf(reset_reason, "Deepsleep reset");
            break;
        case MPSLEEP_SOFT_RESET:
            sprintf(reset_reason, "Soft reset");
            break;
        case MPSLEEP_BROWN_OUT_RESET:
            sprintf(reset_reason, "Brownout reset");
            break;
        default:
            sprintf(reset_reason, "Unknown reset reason");
            break;
    }
}

void mpsleep_get_wake_desc (char *wake_reason) {
    switch (mpsleep_wake_reason) {
        case MPSLEEP_PWRON_WAKE:
            sprintf(wake_reason, "Power on wake");
            break;
        case MPSLEEP_GPIO_WAKE:
            sprintf(wake_reason, "GPIO wake");
            break;
        case MPSLEEP_RTC_WAKE:
            sprintf(wake_reason, "RTC wake");
            break;
        case MPSLEEP_ULP_WAKE:
            sprintf(wake_reason, "ULP wake");
            break;
        default:
            sprintf(wake_reason, "Unknown wake reason");
            break;
    }
}
