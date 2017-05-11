
#ifndef MICROPY_INCLUDED_ESP32_MACINE_RTC_H
#define MICROPY_INCLUDED_ESP32_MACINE_RTC_H

typedef struct {
    uint64_t ext1_pins; // set bit == pin#
    int8_t ext0_pin;   // just the pin#, -1 == None
    bool wake_on_touch : 1;
    bool ext0_level : 1;
    bool ext1_level : 1;
} machine_rtc_config_t;


#endif
