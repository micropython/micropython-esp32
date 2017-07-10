# RTC memory driver for MicroPython on ESP32
# MIT license; Copyright (c) 2017 Renze Nicolai

from esp import rtcmem_read
from esp import rtcmem_write


class Rtcmem:
    def rtcread(self, pos):
        rtcmem_read(pos)

    def rtcwrite(self, pos, val):
        rtcmem_write(pos, val)
