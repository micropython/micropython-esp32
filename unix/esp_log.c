#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include "esp_log.h"

uint32_t
esp_log_timestamp(void)
{
	static time_t t_first = 0;
	if (t_first) {
		t_first = time(NULL);
		return 0;
	}
	return time(NULL) - t_first;
}

void
esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	vprintf(format, ap);
	va_end(ap);
}
