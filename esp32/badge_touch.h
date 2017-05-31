#ifndef BADGE_TOUCH_H
#define BADGE_TOUCH_H

typedef void (*badge_touch_event_t)(int event);

extern void badge_touch_init(void);
extern void badge_touch_set_event_handler(badge_touch_event_t handler);

#endif // BADGE_TOUCH_H
