import machine

pin = machine.Pin(25)
rtc = machine.RTC()
rtc.wake_on_ext0(pin = pin, level = 0)

def start_sleeping(time=0):
    machine.deepsleep(time)

def reboot():
    machine.deepsleep(1)
