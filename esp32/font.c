#include "sdkconfig.h"

#include <stdbool.h>
#include <stdint.h>

#include "font.h"
#include "font_16px.h"
#include "font_8px.h"

static inline void
draw_pixel(uint8_t *buf, int x, int y, int c)
{
	if (x < 0 || x > 295 || y < 0 || y > 127)
		return;
	int pos = (x >> 3) + y * (296 / 8);
	if (c < 128)
		buf[pos] &= 0xff - (1 << (x&7));
	else
		buf[pos] |= (1 << (x&7));
}

static inline void
draw_pixel_8b(uint8_t *buf, int x, int y, int c)
{
	if (x < 0 || x > 295 || y < 0 || y > 127)
		return;
	int pos = x + y * 296;
	buf[pos] = c;
}

int
draw_font(uint8_t *buf, int x, int y, int x_len, const char *text, uint8_t flags) {
	// select font definitions
	const uint8_t *font_data;
	const uint8_t *font_width;
	int font_FIRST, font_LAST, font_WIDTH, font_HEIGHT;

	if (flags & FONT_16PX) {
		font_data = font_16px_data;
		font_width = font_16px_width;
		font_FIRST = FONT_16PX_FIRST;
		font_LAST = FONT_16PX_LAST;
		font_WIDTH = FONT_16PX_WIDTH;
		font_HEIGHT = FONT_16PX_HEIGHT;
	} else {
		font_data = font_8px_data;
		font_width = font_8px_width;
		font_FIRST = FONT_8PX_FIRST;
		font_LAST = FONT_8PX_LAST;
		font_WIDTH = FONT_8PX_WIDTH;
		font_HEIGHT = FONT_8PX_HEIGHT;
	}

	int x_max = x_len + x;

	uint8_t ch_ul = 0x00;
	if (flags & FONT_UNDERLINE_1)
		ch_ul |= 0x80;
	if (flags & FONT_UNDERLINE_2)
		ch_ul |= 0x40;

	int numChars = 0;
	while (*text != 0 && x < x_max) {
		int ch = *text;
		if (ch < font_FIRST || ch > font_LAST)
			ch = 0;
		else
			ch -= font_FIRST;

		uint8_t ch_width;
		unsigned int f_index = ch * (font_WIDTH * font_HEIGHT);
		if (flags & FONT_MONOSPACE) {
			ch_width = font_WIDTH;
		} else {
			ch_width = font_width[ch];
			f_index += (ch_width >> 4) * font_HEIGHT;
			ch_width &= 0x0f;
		}

		if (x + ch_width >= x_max)
			break; // not enough space for full character

		while (ch_width > 0 && x < x_max) {
			int ch_y;
			for (ch_y = 0; ch_y < font_HEIGHT; ch_y++) {
				uint8_t ch = font_data[f_index++];
				if (flags & FONT_INVERT)
					ch = ~ch;
				ch ^= (ch_y == 0 ? ch_ul : 0);

				int ch_y2;
				for (ch_y2=0; ch_y2<8; ch_y2++)
				{
					draw_pixel(buf, x, y + (font_HEIGHT-ch_y-1)*8 + ch_y2, (ch & 1) ? 255 : 0);
					ch >>= 1;
				}
			}
			x++;
			ch_width--;
		}
		text = &text[1];
		numChars++;
	}

	if (flags & FONT_FULL_WIDTH) {
		uint8_t ch = 0;
		if (flags & FONT_INVERT)
			ch = ~ch;
		while (x < x_max) {
			int ch_y;
			for (ch_y = 0; ch_y < font_HEIGHT; ch_y++)
			{
				uint8_t ch2 = ch;
				ch2 = ch ^ (ch_y == 0 ? ch_ul : 0);

				int ch_y2;
				for (ch_y2=0; ch_y2<8; ch_y2++)
				{
					draw_pixel(buf, x, y + (font_HEIGHT-ch_y-1)*8 + ch_y2, (ch2 & 1) ? 255 : 0);
					ch2 >>= 1;
				}
			}
			x++;
		}
	}

	return numChars;
}
