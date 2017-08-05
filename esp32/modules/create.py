import uos

def boot():
    print("Updating 'boot.py'...")
    with open("/boot.py", "w") as f:
            f.write("""\
# This file is executed on every boot (including wake-boot from deepsleep)
import badge, machine, esp, ugfx, sys
badge.init()
ugfx.init()
esp.rtcmem_write(0,0)
esp.rtcmem_write(1,0)
splash = badge.nvs_get_str('boot','splash','splash')
if machine.reset_cause() != machine.DEEPSLEEP_RESET:
    print('[BOOT] Cold boot')
else:
    print("[BOOT] Wake from sleep")
    load_me = esp.rtcmem_read_string()
    if load_me:
        splash = load_me
        print("starting %s" % load_me)
        esp.rtcmem_write_string("")
try:
    if not splash=="shell":
        __import__(splash)
    else:
        ugfx.clear(ugfx.WHITE)
        ugfx.flush(ugfx.LUT_FULL)
except BaseException as e:
    sys.print_exception(e)
    import easydraw
    easydraw.msg("A fatal error occured!","Still Crashing Anyway", True)
    easydraw.msg("")
    easydraw.msg("Guru meditation:")
    easydraw.msg(str(e))
    easydraw.msg("")
    easydraw.msg("Rebooting in 5 seconds...")
    import time
    time.sleep(5)
    import appglue
    appglue.home()
    """)
