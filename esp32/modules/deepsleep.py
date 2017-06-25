# RTC memory driver for MicroPython on ESP32
# MIT license; Copyright (c) 2017 Renze Nicolai

import machine as m

p = m.Pin(25)
r = m.RTC()
r.wake_on_ext0(pin = p, level = 0)

def start_sleeping(self, time=0):
    m.deepsleep(time)

def reboot(self):
    m.deepsleep(1)
