#ifndef ESP_LOG_H
#define ESP_LOG_H

/* based on Espressif's esp-idf code */

#include <stdint.h>
#include <stdarg.h>
#include "sdkconfig.h"

typedef enum {
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE
} esp_log_level_t;

uint32_t esp_log_timestamp(void);

void esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...) __attribute__ ((format (printf, 3, 4)));


#if CONFIG_LOG_COLORS
#define LOG_COLOR_BLACK   "30"
#define LOG_COLOR_RED     "31"
#define LOG_COLOR_GREEN   "32"
#define LOG_COLOR_BROWN   "33"
#define LOG_COLOR_BLUE    "34"
#define LOG_COLOR_PURPLE  "35"
#define LOG_COLOR_CYAN    "36"
#define LOG_COLOR(COLOR)  "\033[0;" COLOR "m"
#define LOG_BOLD(COLOR)   "\033[1;" COLOR "m"
#define LOG_RESET_COLOR   "\033[0m"
#define LOG_COLOR_E       LOG_COLOR(LOG_COLOR_RED)
#define LOG_COLOR_W       LOG_COLOR(LOG_COLOR_BROWN)
#define LOG_COLOR_I       LOG_COLOR(LOG_COLOR_GREEN)
#define LOG_COLOR_D
#define LOG_COLOR_V
#else //CONFIG_LOG_COLORS
#define LOG_COLOR_E
#define LOG_COLOR_W
#define LOG_COLOR_I
#define LOG_COLOR_D
#define LOG_COLOR_V
#define LOG_RESET_COLOR
#endif //CONFIG_LOG_COLORS

#define LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " (%d) %s: " format LOG_RESET_COLOR "\n"

#ifndef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL  ((esp_log_level_t) CONFIG_LOG_DEFAULT_LEVEL)
#endif

#define ESP_EARLY_LOGE( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_ERROR)   { ets_printf(LOG_FORMAT(E, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
#define ESP_EARLY_LOGW( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_WARN)    { ets_printf(LOG_FORMAT(W, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
#define ESP_EARLY_LOGI( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_INFO)    { ets_printf(LOG_FORMAT(I, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
#define ESP_EARLY_LOGD( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)   { ets_printf(LOG_FORMAT(D, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
#define ESP_EARLY_LOGV( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) { ets_printf(LOG_FORMAT(V, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }

#define ESP_LOGE( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_ERROR)   { esp_log_write(ESP_LOG_ERROR,   tag, LOG_FORMAT(E, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
#define ESP_LOGW( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_WARN)    { esp_log_write(ESP_LOG_WARN,    tag, LOG_FORMAT(W, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
#define ESP_LOGI( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_INFO)    { esp_log_write(ESP_LOG_INFO,    tag, LOG_FORMAT(I, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
#define ESP_LOGD( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)   { esp_log_write(ESP_LOG_DEBUG,   tag, LOG_FORMAT(D, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }
#define ESP_LOGV( tag, format, ... )  if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE) { esp_log_write(ESP_LOG_VERBOSE, tag, LOG_FORMAT(V, format), esp_log_timestamp(), tag, ##__VA_ARGS__); }

#endif // ESP_LOG_H
