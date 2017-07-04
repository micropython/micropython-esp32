import ugfx
import badge
import sys
import gc
import uos as os
import uerrno as errno
import ujson as json
import ussl
import usocket
import time
import network
import machine

sta_if = network.WLAN(network.STA_IF); sta_if.active(True)
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

ugfx.input_init()

debug = False
warn_ussl = True

def url_open(url):
    global warn_ussl

    if debug:
        print(url)

    proto, _, host, urlpath = url.split('/', 3)
    try:
        ai = usocket.getaddrinfo(host, 443)
    except OSError as e:
        fatal("Unable to resolve %s (no Internet?)" % host, e)
    #print("Address infos:", ai)
    addr = ai[0][4]

    s = usocket.socket(ai[0][0])
    try:
        #print("Connect address:", addr)
        s.connect(addr)

        if proto == "https:":
            s = ussl.wrap_socket(s, server_hostname=host)
            if warn_ussl:
                print("Warning: %s SSL certificate is not validated" % host)
                warn_ussl = False

        # MicroPython rawsocket module supports file interface directly
        s.write("GET /%s HTTP/1.0\r\nHost: %s\r\n\r\n" % (urlpath, host))
        l = s.readline()
        protover, status, msg = l.split(None, 2)
        if status != b"200":
            if status == b"404" or status == b"301":
                raise NotFoundError("Package not found")
            raise ValueError(status)
        while 1:
            l = s.readline()
            if not l:
                raise ValueError("Unexpected EOF in HTTP headers")
            if l == b'\r\n':
                break
    except Exception as e:
        s.close()
        raise e

    return s

options = ugfx.List(0,0,int(ugfx.width()/2),ugfx.height())

f = url_open("https://badge.sha2017.org/eggs/list/json")
try:
    packages = json.load(f)
finally:
    f.close()

for package in packages:
    options.add_item("%s rev. %s"% (package["name"], package["revision"]))

ugfx.set_lut(ugfx.LUT_FULL)
ugfx.flush()

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
        machine.deepsleep(1)

ugfx.input_attach(ugfx.JOY_UP, show_description)
ugfx.input_attach(ugfx.JOY_DOWN, show_description)
ugfx.input_attach(ugfx.BTN_A, woezel_it)
ugfx.input_attach(ugfx.BTN_B, woezel_it)

ugfx.input_attach(ugfx.BTN_START, start_app)

ugfx.input_attach(ugfx.JOY_LEFT, lambda pushed: ugfx.flush() if pushed else 0)
ugfx.input_attach(ugfx.JOY_RIGHT, lambda pushed: ugfx.flush() if pushed else 0)

text = ugfx.Textbox(int(ugfx.width()/2),0, int(ugfx.width()/2), ugfx.height())

ugfx.flush()
time.sleep(0.2)
ugfx.set_lut(ugfx.LUT_FASTER)
