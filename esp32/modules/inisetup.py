import uos
from flashbdev import bdev

def check_bootsec():
    buf = bytearray(bdev.SEC_SIZE)
    bdev.readblocks(0, buf)
    empty = True
    for b in buf:
        if b != 0xff:
            empty = False
            break
    if empty:
        return True
    fs_corrupted()

def fs_corrupted():
    import time
    while 1:
        print("""\
FAT filesystem appears to be corrupted. If you had important data there, you
may want to make a flash snapshot to try to recover it. Otherwise, perform
factory reprogramming of MicroPython firmware (completely erase flash, followed
by firmware programming).
""")
        time.sleep(3)

def setup():
    check_bootsec()
    print("Performing initial setup")
    uos.VfsFat.mkfs(bdev)
    vfs = uos.VfsFat(bdev)
    uos.mount(vfs, '/flash')
    uos.chdir('/flash')
    with open("boot.py", "w") as f:
        f.write("""\
# This file is executed on every boot (including wake-boot from deepsleep)
import badge, machine, esp, ugfx
badge.init()
ugfx.init()
esp.rtcmem_write(0,0)
esp.rtcmem_write(1,0)
if machine.reset_cause() != machine.DEEPSLEEP_RESET:
    print("cold boot")
    import launcher
else:
    print("wake from sleep")
    load_me = esp.rtcmem_read_string()
    if load_me != "":
        print("starting %s", load_me)
        esp.rtcmem_write_string("")
        __import__(load_me)
    else:
        import launcher
""")
    return vfs
