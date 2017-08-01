import uos

def setup():
    print("Performing initial setup")
    vfs = uos.VfsNative(True)
    with open("/boot.py", "w") as f:
        f.write("""\
# This file is executed on every boot (including wake-boot from deepsleep)
import badge, machine, esp, ugfx
badge.init()
ugfx.init()
esp.rtcmem_write(0,0)
esp.rtcmem_write(1,0)
splash = badge.nvs_get_str('badge','boot.splash','splash')
if machine.reset_cause() != machine.DEEPSLEEP_RESET:
    print('[BOOT.PY] Cold boot')
else:
    print("[BOOT.PY] Wake from sleep")
    load_me = esp.rtcmem_read_string()
    if load_me:
        splash = load_me
        print("starting %s" % load_me)
        esp.rtcmem_write_string("")
__import__(splash)
""")
    return vfs
