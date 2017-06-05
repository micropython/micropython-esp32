# RTC memory driver for MicroPython on ESP32
# MIT license; Copyright (c) 2017 Renze Nicolai

from esp import start_sleeping


class DeepSleep:
    def start_sleeping(self, time=0):
        start_sleeping(time)
        
    def reboot(self):
        start_sleeping(1)
