import ugfx, esp, badge, deepsleep

def start_app(app):
    ugfx.clear(ugfx.WHITE)
    ugfx.string(0, 0, "Please wait...", "PermanentMarker22", ugfx.BLACK)
    if (app==""):
        ugfx.string(0,  25, "Returning to homescreen...","Roboto_Regular12",ugfx.BLACK)
    else:
        ugfx.string(0,  25, "Starting "+app+"...","Roboto_Regular12",ugfx.BLACK)
    ugfx.flush()
    esp.rtcmem_write_string(app)
    badge.eink_busy_wait()
    deepsleep.reboot()

def home():
    start_app("")

def start_ota():
    esp.rtcmem_write(0,1)
    esp.rtcmem_write(1,~1)
    deepsleep.reboot()
