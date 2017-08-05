# File: badgeeventreminder.py
# Version: 1
# Description: Easter egg
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import virtualtimers, time, appglue, badge

whenToTrigger = 1502196900 - 600

def ber_task():
    global whenToTrigger
    now = time.time()
    if now>=whenToTrigger:
        badge.nvs_set_u8('badge','evrt',1)
        print("BADGE EVENT REMINDER ACTIVATED")
        appglue.start_app("badge_event_reminder")
    idleFor = whenToTrigger - now
    if idleFor<0:
        idleFor = 0
    return idleFor

def enable():
    if badge.nvs_get_u8('badge','evrt',0)==0:
        virtualtimers.new(1, ber_task)

def disable():
    virtualtimers.delete(ber_task)
