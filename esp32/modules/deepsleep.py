import machine as m

p = m.Pin(25)
r = m.RTC()
r.wake_on_ext0(pin = p, level = 0)

def start_sleeping(time=0):
    m.deepsleep(time)

def reboot():
    m.deepsleep(1)
