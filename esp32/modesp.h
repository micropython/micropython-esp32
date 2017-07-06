void esp_neopixel_write(uint8_t pin, uint8_t *pixels, uint32_t numBytes,
                        uint8_t timing);
void esp_rtcmem_write(uint32_t pos, uint8_t val);
uint8_t esp_rtcmem_read(uint32_t pos);
void esp_start_sleeping(int time);
