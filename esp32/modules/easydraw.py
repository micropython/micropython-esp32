# File: easydraw.py
# Version: 1
# Description: Wrapper that makes drawing things simple
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import ugfx, badge

# Globals
msgLineNumber = 0
msgShown = False

# Functions
def msg(message, clear=False):
    global msgShown
    msgShown = True
    global msgLineNumber
    if clear:
        ugfx.clear(ugfx.WHITE)
        ugfx.string(0, 0, message, "PermanentMarker22", ugfx.BLACK)
        msgLineNumber = 0
    else:
        ugfx.string(0, 30 + (msgLineNumber * 15), message, "Roboto_Regular12", ugfx.BLACK)
        ugfx.flush(ugfx.LUT_FASTER)
        msgLineNumber += 1

def nickname(y = 25, font = "PermanentMarker36", color = ugfx.BLACK):
    nick = badge.nvs_get_str("owner", "name", 'Jan de Boer')
    ugfx.string_box(0,y,296,38, nick, font, color, ugfx.justifyCenter)
        
def battery(vUsb, vBatt, charging):
    vMin = badge.nvs_get_u16('batt', 'vmin', 3700) # mV
    vMax = badge.nvs_get_u16('batt', 'vmax', 4200) # mV
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
