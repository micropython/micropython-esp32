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

apps = os.listdir('lib')

apps.extend(['installer'])

for app in apps:
    options.add_item(app)

def run_it(pushed):
    if (pushed):
        ugfx.clear(ugfx.BLACK)
        ugfx.string(40,25,"Running:","Roboto_BlackItalic24",ugfx.WHITE)
        ugfx.string(100,75, apps[options.selected_index()],"PermanentMarker22",ugfx.WHITE)
        ugfx.flush()
        __import__(apps[options.selected_index()])

ugfx.input_attach(ugfx.BTN_A, run_it)
ugfx.input_attach(ugfx.BTN_B, run_it)

ugfx.input_attach(ugfx.JOY_UP, lambda x: ugfx.flush())
ugfx.input_attach(ugfx.JOY_DOWN, lambda x: ugfx.flush())

ugfx.flush()
time.sleep(0.2)
ugfx.set_lut(ugfx.LUT_FASTER)
