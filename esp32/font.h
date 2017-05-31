#ifndef EPD_FONT_H
#define EPD_FONT_H

#include <stdbool.h>
#include <stdint.h>

#define FONT_16PX        0x01
#define FONT_INVERT      0x02
#define FONT_FULL_WIDTH  0x04
#define FONT_MONOSPACE   0x08
#define FONT_UNDERLINE_1 0x10
#define FONT_UNDERLINE_2 0x20

extern int
draw_font(uint8_t *buf, int x, int y, int x_len, const char *text, uint8_t flags);

#endif // EPD_FONT_H
