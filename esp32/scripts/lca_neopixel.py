# micropython ESP32 - turn on and off LCD backlight for LCA2017 "badge"
from machine import Pin
from apa106 import APA106
num_pixels = 2


colors = ((255,0,0), (255,0,0), (0,255,0), (0,255,0), (0,0,255), (0,0,255))

button_pin = 0
neo_pin = Pin(23, Pin.OUT)
neo = APA106(neo_pin, num_pixels)
base = -1


pButton = Pin(button_pin, Pin.IN)

last_button = 1
while(1):
if not (last_button == pButton.value() ):
last_button = pButton.value()
base += 1
base = base %len(colors)
print(base)
neo.fill(colors[base] )
neo.write()


def set_colors():

for pix in range(num_pixels):
neo[pix] = colors[ base ]
print("Setting {} to {}".format(pix, neo[pix]))
neo.write()

