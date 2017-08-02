import ugfx, badge, time, appglue

def exit(p):
    if p:
        appglue.home()

ugfx.clear(ugfx.WHITE)
ugfx.string(0, 0, "SHA2017 Badge", "PermanentMarker22", ugfx.BLACK)
ugfx.string(0, 25, "Please join us for a short talk on how the", "Roboto_Regular12", ugfx.BLACK)
ugfx.string(0, 25+13, "official SHA2017 badge was designed,", "Roboto_Regular12", ugfx.BLACK)
ugfx.string(0, 25+13*2, "some nice numbers and an overview of what all", "Roboto_Regular12", ugfx.BLACK)
ugfx.string(0, 25+13*3, "of you are doing with it.", "Roboto_Regular12", ugfx.BLACK)

ugfx.string(0, 25+13*5, "The events starts now, in room 'No'!", "Roboto_Regular12", ugfx.BLACK)
ugfx.string(0, ugfx.height()-13, "Press any key to close reminder.", "Roboto_Regular12", ugfx.BLACK)
ugfx.set_lut(ugfx.LUT_FULL)
ugfx.flush()

ugfx.input_init()
ugfx.input_attach(ugfx.JOY_UP, exit)
ugfx.input_attach(ugfx.JOY_DOWN, exit)
ugfx.input_attach(ugfx.JOY_LEFT, exit)
ugfx.input_attach(ugfx.JOY_RIGHT, exit)
ugfx.input_attach(ugfx.BTN_SELECT, exit)
ugfx.input_attach(ugfx.BTN_START, exit)
ugfx.input_attach(ugfx.BTN_A, exit)
ugfx.input_attach(ugfx.BTN_B, exit)

def led(on):
    ledVal = 0
    if on:
        ledVal = 255
    badge.leds_send_data(bytes([ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal,ledVal]),24)

for i in range(0,30):
    led(True)
    badge.vibrator_activate(0xFF)
    led(False)
    badge.vibrator_activate(0xFF)

appglue.home()
