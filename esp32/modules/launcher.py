import ugfx, badge, sys, gc
import uos as os
import uerrno as errno
import ujson as json
import time
import esp
import appglue

ugfx.init()
ugfx.input_init()

ugfx.clear(ugfx.WHITE)
ugfx.string(180,25,"STILL","Roboto_BlackItalic24",ugfx.BLACK)
ugfx.string(160,50,"Hacking","PermanentMarker22",ugfx.BLACK)
str_len = ugfx.get_string_width("Hacking","PermanentMarker22")
ugfx.line(160, 72, 174 + str_len, 72, ugfx.BLACK)
ugfx.line(170 + str_len, 52, 170 + str_len, 70, ugfx.BLACK)
ugfx.string(170,75,"Anyway","Roboto_BlackItalic24",ugfx.BLACK)
ugfx.string(155,105,"MOTD: NVS","Roboto_Regular18",ugfx.BLACK)

options = ugfx.List(0,0,int(ugfx.width()/2),ugfx.height())

try:
    apps = os.listdir('lib')
except OSError:
    apps = []

options.add_item('installer')
options.add_item('ota_update')

for app in apps:
    options.add_item(app)

def run_it(pushed):
    if (pushed):
        selected = options.selected_text()
        options.destroy()

        ugfx.clear(ugfx.BLACK)
        ugfx.string(40,25,"Running:","Roboto_BlackItalic24",ugfx.WHITE)
        ugfx.string(100,75, selected,"PermanentMarker22",ugfx.WHITE)
        ugfx.flush()
        badge.eink_busy_wait()
        appglue.start_app(selected)

ugfx.input_attach(ugfx.BTN_A, run_it)
ugfx.input_attach(ugfx.BTN_B, run_it)

ugfx.input_attach(ugfx.JOY_UP, lambda pushed: ugfx.flush() if pushed else 0)
ugfx.input_attach(ugfx.JOY_DOWN, lambda pushed: ugfx.flush() if pushed else 0)

ugfx.set_lut(ugfx.LUT_FULL)
ugfx.flush()
ugfx.set_lut(ugfx.LUT_FASTER)
