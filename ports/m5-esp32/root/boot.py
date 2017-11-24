# This file is executed on every boot (including wake-boot from deepsleep)
from machine import Pin
import network, _thread, upip, time, m5

def exists(fname):
  try:
    with open(fname):
      pass
    return True
  except OSError:
    return False

btnA_pin = Pin(39, Pin.IN)
btnB_pin = Pin(38, Pin.IN)
btnC_pin = Pin(37, Pin.IN)

if btnA_pin.value() == False:
  print('Booting Button A is hold, Enable safe mode.')
  m5.print('Booting Button A is hold.\r\nEnable safe mode.', 0, 0)
  if exists('main.py'):
    uos.rename('main.py', '_main.py')
else:
  if exists('_main.py'):
    uos.rename('_main.py', 'main.py')

if btnB_pin.value() == True:
  import m5cloud