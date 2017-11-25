import gc
import uos
from flashbdev import bdev

try:
    if bdev:
        uos.mount(bdev, '/')
except OSError:
    import inisetup
    vfs = inisetup.setup()

gc.collect()

# import m5
# import m5stack

# import machine, ili9341
# from machine import Pin, SPI
# tft_led_pin = machine.Pin(32, machine.Pin.OUT)
# tft_led_pin.value(1)
# spi = machine.SPI(mosi=machine.Pin(23), miso=machine.Pin(19), sck=machine.Pin(18))
# display = ili9341.ILI9341(spi, cs=machine.Pin(14), dc=machine.Pin(27), rst=machine.Pin(33))
# tft=display

# tft.fill(0x1234)
# tft.fill(0x4321)
# tft.fill(0x0000)

# import machine
# import random
# import ili9341
# import time
# import gc
# import colors
# from machine import Pin
# from widgets import TextArea
# from widgets import Graph

# # use VSPI (ID=2) at 80mhz
# spi = machine.SPI(2,
#                   baudrate=32000000,
#                   mosi=Pin(23, Pin.OUT),  # mosi = 23
#                   miso=Pin(19, Pin.IN),   # miso = 25
#                   sck=Pin(18, Pin.OUT))   # sclk = 19

# display = ili9341.ILI9341(
#     spi,
#     cs=Pin(14, Pin.OUT),          # cs   = 22
#     dc=Pin(27, Pin.OUT),          # dc   = 21
#     rst=Pin(33, Pin.OUT),         # rst  = 18
#     width=320,
#     height=240)

# # turn on the backlight
# bl = Pin(32, Pin.OUT)
# bl.value(1)
# lcd=display

from machine import Pin
pin25=Pin(25, Pin.OUT);
pin25.value(0);