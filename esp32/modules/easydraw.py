# File: easydraw.py
# Version: 1
# Description: Wrapper that makes drawing things simple
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import ugfx, badge

# Functions
def msg(message, title = 'Still Loading Anyway...', reset = False):
    """Show a terminal style loading screen with title

    title can be optionaly set when resetting or first call
    """
    global messageHistory

    try:
        messageHistory
        if reset:
            raise exception
    except:
        ugfx.clear(ugfx.WHITE)
        ugfx.string(0, 0, title, "PermanentMarker22", ugfx.BLACK)
        messageHistory = []

    if len(messageHistory)<6:
        ugfx.string(0, 30 + (len(messageHistory) * 15), message, "Roboto_Regular12", ugfx.BLACK)
        messageHistory.append(message)
    else:
        messageHistory.pop(0)
        messageHistory.append(message)
        ugfx.area(0,30, 296, 98, ugfx.WHITE)
        for i, message in enumerate(messageHistory):
            ugfx.string(0, 30 + (i * 15), message, "Roboto_Regular12", ugfx.BLACK)

    ugfx.flush(ugfx.LUT_FASTER)

def nickname(y = 25, font = "PermanentMarker36", color = ugfx.BLACK):
    nick = badge.nvs_get_str("owner", "name", 'Henk de Vries')
    ugfx.string_box(0,y,296,38, nick, font, color, ugfx.justifyCenter)

def battery(vUsb, vBatt, charging):
    vMin = badge.nvs_get_u16('batt', 'vmin', 3500) # mV
    vMax = badge.nvs_get_u16('batt', 'vmax', 4100) # mV
    if charging and vUsb>4000:
        try:
            badge.eink_png(0,0,'/lib/resources/chrg.png')
        except:
            ugfx.string(0, 0, "CHRG",'Roboto_Regular12',ugfx.BLACK)
    elif vUsb>4000:
        try:
            badge.eink_png(0,0,'/lib/resources/usb.png')
        except:
            ugfx.string(0, 0, "USB",'Roboto_Regular12',ugfx.BLACK)
    else:
        width = round((vBatt-vMin) / (vMax-vMin) * 44)
        if width < 0:
            width = 0
        elif width > 38:
            width = 38
        ugfx.box(2,2,46,18,ugfx.BLACK)
        ugfx.box(48,7,2,8,ugfx.BLACK)
        ugfx.area(3,3,width,16,ugfx.BLACK)
