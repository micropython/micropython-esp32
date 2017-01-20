# micropython ESP32 - turn on and off LCD backlight for LCA2017 "badge"
from machine import Pin, I2C
button_pin = 0
i2c_clock_pin = 22
i2c_data_pin = 21
frequency = 100000

exp_address = 0x20
i2c = I2C(scl=Pin(i2c_clock_pin), sda=Pin(i2c_data_pin), freq=frequency)
pButton = Pin(button_pin, Pin.In)

buff = bytearray(1)
while(1):
	buff[0] = pButton.value() * 255
	i2c.writeto(exp_address, buff)	