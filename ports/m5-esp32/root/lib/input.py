
# This file is part of MicroPython M5Stack package
# Copyright (c) 2017 Mika Tuupola
#
# Licensed under the MIT license:
#   http://www.opensource.org/licenses/mit-license.php
#
# Project home:
#   https://github.com/tuupola/micropython-m5stacj

from machine import Pin

class DigitalInput(object):

    def __init__(self, pin, callback=None, trigger=Pin.IRQ_FALLING):
        if callback is None:
            callback = self.callback
        self.pin = pin
        self.pin.init(self.pin.IN)
        self.pin.irq(trigger=trigger, handler=callback)

    def callback(self, pin):
        pass
