# File: easyrtc.py
# Version: 1
# Description: Wrapper that makes using the clock simple
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import machine, time

# Functions
def string(print_date=False, print_time=True, timestamp = -1):
    if timestamp<0:
        [year, month, mday, wday, hour, minute, second, usec] = machine.RTC().datetime()
    else:
        [year, month, mday, hour, minute, second, wday, yday] = time.localtime(timestamp)
    monthstr = str(month)
    if (month<10):
      monthstr = "0"+monthstr
    daystr = str(mday)
    if (mday<10):
      daystr = "0"+daystr
    hourstr = str(hour)
    if (hour<10):
      hourstr = "0"+hourstr
    minstr = str(minute)
    if (minute<10):
      minstr = "0"+minstr 
    output = ""
    if print_date:
        output += daystr+"-"+monthstr+"-"+str(year)
        if time:
            output += " "
    if print_time:
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
