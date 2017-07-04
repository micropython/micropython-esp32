import ugfx
import badge
import sys
import gc
import uos as os
import uerrno as errno
import ujson as json
import time

ugfx.init()
ugfx.input_init()

ugfx.clear(ugfx.WHITE)
ugfx.string(180,25,"STILL","Roboto_BlackItalic24",ugfx.BLACK)
ugfx.string(160,50,"Hacking","PermanentMarker22",ugfx.BLACK)
str_len = ugfx.get_string_width("Hacking","PermanentMarker22")
ugfx.line(160, 72, 174 + str_len, 72, ugfx.BLACK)
ugfx.line(170 + str_len, 52, 170 + str_len, 70, ugfx.BLACK)
ugfx.string(170,75,"Anyway","Roboto_BlackItalic24",ugfx.BLACK)

options = ugfx.List(0,0,int(ugfx.width()/2),ugfx.height())

try:
    apps = os.listdir('lib')
except OSError:
    apps = []

apps.extend(['installer'])

for app in apps:
    options.add_item(app)

def run_it(pushed):
    if (pushed):
        selected = options.selected_index()
        options.destroy()

        ugfx.clear(ugfx.BLACK)
        ugfx.string(40,25,"Running:","Roboto_BlackItalic24",ugfx.WHITE)
        ugfx.string(100,75, apps[selected],"PermanentMarker22",ugfx.WHITE)
        ugfx.flush()
        __import__(apps[selected])

ugfx.input_attach(ugfx.BTN_A, run_it)
ugfx.input_attach(ugfx.BTN_B, run_it)

ugfx.input_attach(ugfx.JOY_UP, lambda pushed: ugfx.flush() if pushed else 0)
ugfx.input_attach(ugfx.JOY_DOWN, lambda pushed: ugfx.flush() if pushed else 0)

ugfx.flush()
time.sleep(0.2)
ugfx.set_lut(ugfx.LUT_FASTER)
