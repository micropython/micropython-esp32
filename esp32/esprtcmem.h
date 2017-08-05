#ifndef ESPRTCMEM_H
#define ESPRTCMEM_H

#include <stdint.h>

#include <rom/rtc.h>
#include <esp_attr.h>

#define USER_RTC_MEM_SIZE 1024

/**
 * Read byte from rtcmem on offset location.
 *
 * @param location  The offset in the rtcmem.
 * @return 0..255 is ok; -1 is error
 */
extern int esp_rtcmem_read(uint32_t location);

/**
 * Write byte to rtcmem on offset location.
 *
 * @param location  The offset in the rtcmem
 * @param value     The value to write to this offset
 * @return 0 is ok; -1 is error
 */
extern int esp_rtcmem_write(uint32_t location, uint8_t value);

/**
 * Read zero-terminated string from rtcmem on offset location
 *
 * @param location  The offset in the rtcmem
 * @param buffer    The destination buffer
 * @param buf_len   The length of the destination buffer
 *   on input: the total buffer-length
 *   on output: the used buffer-length
 * @return 0 is ok; -1 is error
 */
extern int esp_rtcmem_read_string(uint32_t location, char *buffer, size_t *buf_len);

/**
 * Write zero-terminated string to rtcmem on offfset location
 *
 * @param location  The offset in the rtcmem
 * @param buffer    The string to write to this offset
 * @return 0 is ok; -1 is error
 */
extern int esp_rtcmem_write_string(uint32_t location, const char *buffer);

#endif // ESPRTCMEM_H
