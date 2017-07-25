import ugfx, esp, badge, deepsleep

def start_app(app, display = True):
    if display:
        ugfx.clear(ugfx.WHITE)
        ugfx.string(0, 0, "Loading...", "PermanentMarker22", ugfx.BLACK)
        if app:
            ugfx.string(0,  25, "Starting "+app+"...","Roboto_Regular12",ugfx.BLACK)
        else:
            ugfx.string(0,  25, "Returning to homescreen...","Roboto_Regular12",ugfx.BLACK)
        ugfx.flush(ugfx.LUT_FASTER)
    esp.rtcmem_write_string(app)
    badge.eink_busy_wait()
    deepsleep.reboot()

def home():
    start_app("")

def start_ota():
    esp.rtcmem_write(0,1)
    esp.rtcmem_write(1,254)
    deepsleep.reboot()

def start_bpp():
    esp.rtcmem_write(0,2)
    esp.rtcmem_write(1,253)
    deepsleep.reboot()
