import ugfx, badge, sys, gc
import uos as os
import uerrno as errno
import ujson as json
import network
import machine, esp, time
import urequests as requests

sta_if = network.WLAN(network.STA_IF)
badge.wifi_init()

ugfx.clear(ugfx.BLACK)
ugfx.string(20,25,"Connecting to:","Roboto_BlackItalic24",ugfx.WHITE)
ugfx.string(140,75, "WiFi","PermanentMarker22",ugfx.WHITE)
ugfx.flush()

while not sta_if.isconnected():
    time.sleep(0.1)
    pass

ugfx.clear(ugfx.WHITE)
ugfx.string(180,25,"STILL","Roboto_BlackItalic24",ugfx.BLACK)
ugfx.string(160,50,"Hacking","PermanentMarker22",ugfx.BLACK)
str_len = ugfx.get_string_width("Hacking","PermanentMarker22")
ugfx.line(160, 72, 174 + str_len, 72, ugfx.BLACK)
ugfx.line(170 + str_len, 52, 170 + str_len, 70, ugfx.BLACK)
ugfx.string(170,75,"Anyway","Roboto_BlackItalic24",ugfx.BLACK)

def show_description(active):
    if active:
         text.text(packages[options.selected_index()]["description"])
         ugfx.flush()

def woezel_it(active):
    if active:
        ugfx.clear(ugfx.BLACK)
        ugfx.string(40,25,"Installing:","Roboto_BlackItalic24",ugfx.WHITE)
        ugfx.string(100,75, packages[options.selected_index()]["name"],"PermanentMarker22",ugfx.WHITE)
        ugfx.flush()
        import woezel
        woezel.install(packages[options.selected_index()]["slug"])
        ugfx.clear(ugfx.BLACK)
        ugfx.string(40,25,"Installed:","Roboto_BlackItalic24",ugfx.WHITE)
        ugfx.string(100,75, packages[options.selected_index()]["name"],"PermanentMarker22",ugfx.WHITE)
        ugfx.flush()

def start_app(pushed):
    if(pushed):
        ugfx.clear(ugfx.BLACK)
        ugfx.string(40,25,"Running:","Roboto_BlackItalic24",ugfx.WHITE)
        ugfx.string(100,75, packages[options.selected_index()]["name"],"PermanentMarker22",ugfx.WHITE)
        ugfx.flush()
        selected = packages[options.selected_index()]["slug"]
        esp.rtcmem_write_string(selected)
        badge.eink_busy_wait()
        esp.start_sleeping(1)

ugfx.input_init()

window = ugfx.Container(0, 0, ugfx.width(), ugfx.height())

ugfx.input_attach(ugfx.JOY_UP, show_description)
ugfx.input_attach(ugfx.JOY_DOWN, show_description)
ugfx.input_attach(ugfx.BTN_A, woezel_it)
ugfx.input_attach(ugfx.BTN_B, woezel_it)

ugfx.input_attach(ugfx.BTN_START, start_app)

ugfx.input_attach(ugfx.JOY_LEFT, lambda pushed: ugfx.flush() if pushed else 0)
ugfx.input_attach(ugfx.JOY_RIGHT, lambda pushed: ugfx.flush() if pushed else 0)

text = ugfx.Textbox(int(ugfx.width()/2),0, int(ugfx.width()/2), ugfx.height())

ugfx.set_lut(ugfx.LUT_FULL)
ugfx.flush()
badge.eink_busy_wait()
ugfx.set_lut(ugfx.LUT_FASTER)

gc.collect()

f = requests.get("https://badge.sha2017.org/eggs/list/json")
try:
    packages = f.json()
finally:
    f.close()

gc.collect()

options = ugfx.List(0,0,int(ugfx.width()/2),ugfx.height())

for package in packages:
    options.add_item("%s rev. %s"% (package["name"], package["revision"]))

show_description(True)
