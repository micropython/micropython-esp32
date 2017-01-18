# micropython ESP32 - turn on and off LCD backlight for LCA2017 "badge"
from machine import Pin
from neopixel import NeoPixel
num_pixels = 2

colors = ((255,0,0), (75,0,130))
button_pin = 0
neo_pin = Pin(23, Pin.OUT)
neo = NeoPixel(neo_pin, num_pixels)
base = 0

def set_colors():

for pix in range(num_pixels):
print("Setting {} to {}".format(pix, colors[(pix + base) %2]))
neo[pix] = colors[(pix + base) % 2]


pButton = Pin(button_pin, Pin.IN)

buff = bytearray(1)
last_button = 0
while(1):
if not (last_button == pButton.value() ):
last_button = pButton.value()
base += 1
base = base %2
print(base)
set_colors()
neo.write()
