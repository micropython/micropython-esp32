# File: easyrtc.py
# Version: 1
# Description: Wrapper that makes using the clock simple
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import machine, time

# Functions
def string(date=False, time=True):
    [year, month, mday, wday, hour, min, sec, usec] = machine.RTC().datetime()
    monthstr = str(month)
    if (month<10):
      monthstr = "0"+monthstr
    daystr = str(mday)
    if (mday<10):
      daystr = "0"+daystr
    hourstr = str(hour)
    if (hour<10):
      hourstr = "0"+hourstr
    minstr = str(min)
    if (min<10):
      minstr = "0"+minstr 
    output = ""
    if date:
        output += daystr+"-"+monthstr+"-"+str(year)
        if time:
            output += " "
    if time:
        output += hourstr+":"+minstr
    return output

def configure():
    import easywifi
    import easydraw
    import ntp
    if not easywifi.status():
        if not easywifi.enable():
            return False
    easydraw.msg("Configuring clock...")
    ntp.set_NTP_time()
    easydraw.msg("Done")
    return True
