# File: easydraw.py
# Version: 1
# Description: Wrapper that makes drawing things simple
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import ugfx

# Globals
msgLineNumber = 0

# Functions
def msg(message, clear=False):
    global msgLineNumber
    if clear:
        ugfx.clear(ugfx.WHITE)
        ugfx.string(0, 0, message, "PermanentMarker22", ugfx.BLACK)
        ugfx.set_lut(ugfx.LUT_FASTER)
        ugfx.flush()
        msgLineNumber = 0
    else:
        ugfx.string(0, 30 + (msgLineNumber * 15), message, "Roboto_Regular12", ugfx.BLACK)
        ugfx.flush()
        msgLineNumber += 1
