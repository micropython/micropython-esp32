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
import machine
if machine.reset_cause() == machine.DEEPSLEEP_RESET:
    import badge
    import ugfx

    badge.init()
    ugfx.init()

    ugfx.clear(ugfx.BLACK)

    ugfx.fill_circle(60, 60, 50, ugfx.WHITE);
    ugfx.fill_circle(60, 60, 40, ugfx.BLACK);
    ugfx.fill_circle(60, 60, 30, ugfx.WHITE);
    ugfx.fill_circle(60, 60, 20, ugfx.BLACK);
    ugfx.fill_circle(60, 60, 10, ugfx.WHITE);

    ugfx.thickline(1,1,100,100,ugfx.WHITE,10,5)
    ugfx.box(30,30,50,50,ugfx.WHITE)

    ugfx.string(150,25,"STILL","Roboto_BlackItalic24",ugfx.WHITE)
    ugfx.string(130,50,"Hacking","PermanentMarker22",ugfx.WHITE)
    len = ugfx.get_string_width("Hacking","PermanentMarker22")
    ugfx.line(130, 72, 144 + len, 72, ugfx.WHITE)
    ugfx.line(140 + len, 52, 140 + len, 70, ugfx.WHITE)
    ugfx.string(140,75,"Anyway","Roboto_BlackItalic24",ugfx.WHITE)

    ugfx.flush()

    def render(text, pushed):
        if(pushed):
            ugfx.string(100,10,text,"PermanentMarker22",ugfx.WHITE)
        else:
            ugfx.string(100,10,text,"PermanentMarker22",ugfx.BLACK)
        ugfx.flush()

    ugfx.input_init()
    ugfx.input_attach(ugfx.JOY_UP, lambda pressed: render('UP', pressed))
    ugfx.input_attach(ugfx.JOY_DOWN, lambda pressed: render('DOWN', pressed))
    ugfx.input_attach(ugfx.JOY_LEFT, lambda pressed: render('LEFT', pressed))
    ugfx.input_attach(ugfx.JOY_RIGHT, lambda pressed: render('RIGHT', pressed))
    ugfx.input_attach(ugfx.BTN_A, lambda pressed: render('A', pressed))
    ugfx.input_attach(ugfx.BTN_B, lambda pressed: render('B', pressed))
    ugfx.input_attach(ugfx.BTN_START, lambda pressed: render('Start', pressed))
    ugfx.input_attach(ugfx.BTN_SELECT, lambda pressed: render('Select', pressed))
""")
    return vfs
