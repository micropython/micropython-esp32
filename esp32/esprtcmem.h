#ifndef ESPRTCMEM_H
#define ESPRTCMEM_H

#include "esp_attr.h"
#include "rom/rtc.h"

#define USER_RTC_MEM_SIZE 256

const uint8_t esp_rtcmem_read(uint32_t location);
void esp_rtcmem_write(uint32_t location, uint8_t value);
void esp_rtcmem_read_string(uint32_t location, char *buffer);
void esp_rtcmem_write_string(uint32_t location, const char *buffer);

#endif
