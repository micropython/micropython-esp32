#ifndef BADGE_MPR121_H
#define BADGE_MPR121_H

typedef void (*badge_mpr121_intr_t)(void*);

extern void badge_mpr121_init(void);

extern void badge_mpr121_set_interrupt_handler(uint8_t pin, badge_mpr121_intr_t handler, void *arg);

extern int badge_mpr121_get_interrupt_status(void);

// all possible settings
#define MPR121_DISABLED         0x00
#define MPR121_INPUT            0x08
#define MPR121_INPUT_PULL_DOWN  0x09
#define MPR121_INPUT_PULL_UP    0x0b
#define MPR121_OUTPUT           0x0c
#define MPR121_OUTPUT_LOW_ONLY  0x0e
#define MPR121_OUTPUT_HIGH_ONLY 0x0f
extern int badge_mpr121_configure_gpio(int pin, int config);

extern int badge_mpr121_get_gpio_level(int pin);
extern int badge_mpr121_set_gpio_level(int pin, int value);

#endif // BADGE_MPR121_H
