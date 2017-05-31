#ifndef BADGE_EINK_H
#define BADGE_EINK_H

#include <stdint.h>

#define BADGE_EINK_WIDTH  296
#define BADGE_EINK_HEIGHT 128

/* badge_eink_display 'mode' settings */
// bitmapped flags:
#define DISPLAY_FLAG_GREYSCALE  1
#define DISPLAY_FLAG_ROTATE_180 2
#define DISPLAY_FLAG_NO_UPDATE  4
// fields and sizes:
#define DISPLAY_FLAG_LUT_BIT    8
#define DISPLAY_FLAG_LUT_SIZE   4

extern void badge_eink_init(void);

struct badge_eink_update {
	int lut;
	const uint8_t *lut_custom;
	int reg_0x3a;
	int reg_0x3b;
	int y_start;
	int y_end;
};
extern const struct badge_eink_update eink_upd_default;

extern void badge_eink_update(const struct badge_eink_update *upd_conf);

/*
 * display image on badge
 *
 * img is 1 byte per pixel; from top-left corner in horizontal
 * direction first (296 pixels).
 *
 * mode is still work in progress. think of:
 * - fast update
 * - slow/full update
 * - greyscale update
 */
extern void badge_eink_display(const uint8_t *img, int mode);

#endif // BADGE_EINK_H
