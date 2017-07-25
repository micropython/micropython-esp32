/*
 * This file is part of the SHA2017 Badge MicroPython project,
 * http://micropython.org/  https://badge.sha2017.org
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Renze Nicolai
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

#include <stdint.h>
#include <string.h>

#include <py/mphal.h>

#include "esprtcmem.h"

static uint8_t RTC_DATA_ATTR rtcmemcontents[USER_RTC_MEM_SIZE] = { 0 };

// @return 0..255 is ok; -1 is error
int
esp_rtcmem_read(uint32_t location)
{
	if (location >= USER_RTC_MEM_SIZE)
	{
		return -1;
	}

	return rtcmemcontents[location];
}

// @return 0 is ok; -1 is error
int
esp_rtcmem_write(uint32_t location, uint8_t value)
{
	if (location >= USER_RTC_MEM_SIZE)
	{
		return -1;
	}

	rtcmemcontents[location] = value;

	return 0;
}

// @return 0 is ok; -1 is error
int
esp_rtcmem_read_string(uint32_t location, char *buffer, size_t *buf_len)
{
	if (location >= USER_RTC_MEM_SIZE)
	{
		return -1;
	}

	size_t maxlen = USER_RTC_MEM_SIZE - location;
	size_t len = strnlen((const char *) &rtcmemcontents[location], maxlen);
	if (len == maxlen)
	{
		return -1;
	}

	if (*buf_len <= len)
	{
		return -1;
	}
	*buf_len = len + 1;
	memcpy(buffer, &rtcmemcontents[location], len + 1);

	return 0;
}

// @return 0 is ok; -1 is error
int
esp_rtcmem_write_string(uint32_t location, const char *buffer)
{
	if (location >= USER_RTC_MEM_SIZE || location + strlen(buffer) >= USER_RTC_MEM_SIZE)
	{
		return -1;
	}

	memcpy(&rtcmemcontents[location], buffer, strlen(buffer)+1);

	return 0;
}
