import ugfx, time, ntp, badge, machine, deepsleep, network, esp, gc
import appglue, services

# File: splash.py
# Version: 3
# Description: Homescreen for SHA2017 badge
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>
#          Thomas Roos   <?>


### FUNCTIONS

# RTC
def splash_rtc_string(date=False, time=True):
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

# Graphics
def splash_draw_battery(vUsb, vBatt):
    
    if vBatt>500 and badge.battery_charge_status() and vUsb>4000:
        try:
            badge.eink_png(0,0,'/chrg.png')
        except:
            ugfx.string(0, 0, "CHRG",'Roboto_Regular12',ugfx.BLACK)
    elif vUsb>4000:
        try:
            badge.eink_png(0,0,'/usb.png')
        except:
            ugfx.string(0, 0, "USB",'Roboto_Regular12',ugfx.BLACK)
    elif vBatt<500:
        try:
            badge.eink_png(0,0,'/nobatt.png')
        except:
            ugfx.string(0, 0, "NO BATT",'Roboto_Regular12',ugfx.BLACK)
    else:
        width = round((vBatt-vMin) / (vMax-vMin) * 38)
        if width < 0:
            width = 0
        elif width > 38:
            width = 38
        ugfx.box(2,2,40,18,ugfx.BLACK)
        ugfx.box(42,7,2,8,ugfx.BLACK)
        ugfx.area(3,3,width,16,ugfx.BLACK)

    bat_status = str(round(vBatt/1000, 2)) + 'v'  
    global splashPowerCountdown
    bat_status = bat_status + ' ' + str(splashPowerCountdown)
    ugfx.string(47, 0, bat_status,'Roboto_Regular12',ugfx.BLACK)

def splash_draw_nickname():
    global nick
    ugfx.string(0, 25, nick, "PermanentMarker36", ugfx.BLACK)

def splash_draw_actions(sleeping):
    global otaAvailable
    if sleeping:
        info1 = 'Sleeping...'
        info2 = 'Press any key to wake up'
    else:
        info1 = 'Press start to open the launcher'
        if otaAvailable:
            info2 = 'Press select to start OTA update'
        else:
            info2 = splash_rtc_string(True, True)

    l = ugfx.get_string_width(info1,"Roboto_Regular12")
    ugfx.string(296-l, 0, info1, "Roboto_Regular12",ugfx.BLACK)
    l = ugfx.get_string_width(info2,"Roboto_Regular12")
    ugfx.string(296-l, 12, info2, "Roboto_Regular12",ugfx.BLACK)

def splash_draw(full=False,sleeping=False):    
    global splashDrawMsg
    if splashDrawMsg:
        splashDrawMsg = False
        full = True
        global splashDrawMsgLineNumber
        splashDrawMsgLineNumber = 0
        
    vUsb = badge.usb_volt_sense()
    vBatt = badge.battery_volt_sense()
    vBatt += vDrop
        
    if splash_power_countdown_get()<1:
        full= True
    
    if full:
        ugfx.clear(ugfx.WHITE)
        splash_draw_nickname()
    else:
        ugfx.area(0,0,ugfx.width(),24,ugfx.WHITE)
        ugfx.area(0,ugfx.height()-64,ugfx.width(),64,ugfx.WHITE)
    
    splash_draw_battery(vUsb, vBatt)
    splash_draw_actions(sleeping)
    
    services.draw()
    
    if full:
        ugfx.flush(ugfx.LUT_FULL)
    else:
        ugfx.flush(ugfx.LUT_NORMAL)
    

def splash_draw_msg(message, clear=False):
    global splashDrawMsg
    splashDrawMsg = True
    global splashDrawMsgLineNumber
    try:
        splashDrawMsgLineNumber
    except:
        splashDrawMsgLineNumber = 0
        
    if clear:
        ugfx.clear(ugfx.WHITE)
        ugfx.string(0, 0, message, "PermanentMarker22", ugfx.BLACK)
        ugfx.set_lut(ugfx.LUT_FASTER)
        ugfx.flush()
        splashDrawMsgLineNumber = 0
    else:
        ugfx.string(0, 30 + (splashDrawMsgLineNumber * 15), message, "Roboto_Regular12", ugfx.BLACK)
        ugfx.flush()
        splashDrawMsgLineNumber += 1

# WiFi
def splash_wifi_connect():
    global wifiStatus
    try:
        wifiStatus
    except:
        wifiStatus = False
       
    if not wifiStatus:
        nw = network.WLAN(network.STA_IF)
        if not nw.isconnected():
            nw.active(True)
            ssid = badge.nvs_get_str('badge', 'wifi.ssid', 'SHA2017-insecure')
            password = badge.nvs_get_str('badge', 'wifi.password')
            nw.connect(ssid, password) if password else nw.connect(ssid)

            splash_draw_msg("Connecting to WiFi...", True)
            splash_draw_msg("("+ssid+")")

            timeout = badge.nvs_get_u8('splash', 'wifi.timeout', 40)
            while not nw.isconnected():
                time.sleep(0.1)
                timeout = timeout - 1
                if (timeout<1):
                    splash_draw_msg("Timeout while connecting!")
                    splash_wifi_disable()
                    return False
        wifiStatus = True
        return True
    return False

def splash_wifi_active():
    global wifiStatus
    try:
        wifiStatus
    except:
        wifiStatus = False
    return wifiStatus
        
    
def splash_wifi_disable():
    global wifiStatus
    wifiStatus = False
    nw = network.WLAN(network.STA_IF)
    nw.active(False)
    
# NTP clock configuration
def splash_ntp():
    if not splash_wifi_active():
        if not splash_wifi_connect():
            return False
    splash_draw_msg("Configuring clock...", True)
    ntp.set_NTP_time()
    splash_draw_msg("Done")
    return True
    
# OTA update checking

def splash_ota_download_info():
    import urequests as requests
    splash_draw_msg("Checking for updates...", True)
    result = False
    try:
        data = requests.get("https://badge.sha2017.org/version")
    except:
        splash_draw_msg("Error:")
        splash_draw_msg("Could not download JSON!")
        time.sleep(5)
        return False
    try:
        result = data.json()
    except:
        data.close()
        splash_draw_msg("Error:")
        splash_draw_msg("Could not decode JSON!")
        time.sleep(5)
        return False
    data.close()
    return result

def splash_ota_check():
    if not splash_wifi_active():
        if not splash_wifi_connect():
            return False
        
    info = splash_ota_download_info()
    if info:
        import version
        if info["build"] > version.build:
            badge.nvs_set_u8('badge','OTA.ready',1)
            return True

    badge.nvs_set_u8('badge','OTA.ready',0)
    return False

def splash_ota_start():
    appglue.start_ota()

# About

def splash_about_countdown_reset():
    global splashAboutCountdown
    splashAboutCountdown = badge.nvs_get_u8('splash', 'about.amount', 10)
    
def splash_about_countdown_trigger():
    global splashAboutCountdown
    try:
        splashAboutCountdown
    except:
        splash_about_countdown_reset()

    splashAboutCountdown -= 1
    if splashAboutCountdown<0:
        appglue.start_app('magic', False)
    else:
        print("[SPLASH] Magic in "+str(splashAboutCountdown)+"...")
            

# Power management

def splash_power_countdown_reset():
    global splashPowerCountdown
    splashPowerCountdown = badge.nvs_get_u8('splash', 'timer.amount', 30)

def splash_power_countdown_get():
    global splashPowerCountdown
    try:
        splashPowerCountdown
    except:
        splash_power_countdown_reset()
    return splashPowerCountdown
  
def splash_power_countdown_trigger():
    global splashPowerCountdown
    try:
        splashPowerCountdown
    except:
        splash_power_countdown_reset()
    
    splashPowerCountdown -= 1
    if badge.usb_volt_sense() > 4000:
        splash_power_countdown_reset()
        return False
    elif splashPowerCountdown<0:
        print("[SPLASH] Going to sleep...")
        splash_draw(True,True)
        badge.eink_busy_wait()
        #now = time.time()
        #wake_at = round(now + badge.nvs_get_u16('splash', 'sleep.duration', 120)*1000)
        #appglue.start_bpp()
        deepsleep.start_sleeping(badge.nvs_get_u16('splash', 'sleep.duration', 120)*1000)
        return True
    else:
        print("[SPLASH] Sleep in "+str(splashPowerCountdown)+"...")
        return False


# Button input

def splash_input_start(pressed):
    # Pressing start always starts the launcher
    if pressed:
        print("[SPLASH] Start button pressed")
        appglue.start_app("launcher", False)

def splash_input_a(pressed):
    if pressed:
        print("[SPLASH] A button pressed")
        splash_power_countdown_reset()
        splash_about_countdown_trigger()

def splash_input_select(pressed):
    if pressed:
        print("[SPLASH] Select button pressed")
        global otaAvailable
        if otaAvailable:
            splash_ota_start()
        splash_power_countdown_reset()
        
#def splash_input_left(pressed):
#    if pressed:
#        appglue.start_bpp()

def splash_input_other(pressed):
    if pressed:
        print("[SPLASH] Other button pressed")
        splash_power_countdown_reset()

def splash_input_init():
    print("[SPLASH] Inputs attached")
    ugfx.input_init()
    ugfx.input_attach(ugfx.BTN_START, splash_input_start)
    ugfx.input_attach(ugfx.BTN_A, splash_input_a)
    ugfx.input_attach(ugfx.BTN_B, splash_input_other)
    ugfx.input_attach(ugfx.BTN_SELECT, splash_input_select)
    ugfx.input_attach(ugfx.JOY_UP, splash_input_other)
    ugfx.input_attach(ugfx.JOY_DOWN, splash_input_other)
    ugfx.input_attach(ugfx.JOY_LEFT, splash_input_other)
    ugfx.input_attach(ugfx.JOY_RIGHT, splash_input_other)

# Event timer
def splash_timer_init():
    global splashTimer
    try:
        splashTimer
        print("[SPLASH] Timer exists already")
    except:
        splashTimer = machine.Timer(-1)
        splashTimer.init(period=badge.nvs_get_u16('splash', 'timer.period', 100), mode=machine.Timer.ONE_SHOT, callback=splash_timer_callback)
        print("[SPLASH] Timer created")
    
def splash_timer_callback(tmr):
    try:
        if services.loop(splash_power_countdown_get()):
            splash_power_countdown_reset()
    except:
        pass
    if not splash_power_countdown_trigger():
        splash_draw(False, False)
    tmr.init(period=badge.nvs_get_u16('splash', 'timer.period', 100), mode=machine.Timer.ONE_SHOT, callback=splash_timer_callback)
    
    
### PROGRAM

# Load settings from NVS
nick = badge.nvs_get_str("owner", "name", 'Jan de Boer')
vMin = badge.nvs_get_u16('splash', 'bat.volt.min', 3700) # mV
vMax = badge.nvs_get_u16('splash', 'bat.volt.max', 4200) # mV

# Calibrate battery voltage drop
if badge.battery_charge_status() == False and badge.usb_volt_sense() > 4500 and badge.battery_volt_sense() > 2500:
    badge.nvs_set_u16('splash', 'bat.volt.drop', 5200 - badge.battery_volt_sense()) # mV
    print('Set vDrop to: ' + str(4200 - badge.battery_volt_sense()))
vDrop = badge.nvs_get_u16('splash', 'bat.volt.drop', 1000) - 1000 # mV

# Set global variables
splashDrawMsg = False

# Initialize user input subsystem
splash_input_init()

# Initialize power management subsystem
splash_power_countdown_reset()

# Initialize about subsystem
splash_about_countdown_reset()
    
# Setup / Sponsors / OTA check / NTP clock sync
setupState = badge.nvs_get_u8('badge', 'setup.state', 0)
if setupState == 0: #First boot
    print("[SPLASH] First boot...")
    appglue.start_app("setup")
elif setupState == 1: # Second boot: Show sponsors
    print("[SPLASH] Second boot...")
    badge.nvs_set_u8('badge', 'setup.state', 2)
    appglue.start_app("sponsors")
elif setupState == 2: # Third boot: force OTA check
    print("[SPLASH] Third boot...")
    badge.nvs_set_u8('badge', 'setup.state', 3)
    otaCheck = splash_ntp() if time.time() < 1482192000 else True
    otaAvailable = splash_ota_check()
else: # Normal boot
    print("[SPLASH] Normal boot...")
    otaCheck = splash_ntp() if time.time() < 1482192000 else True
    if (machine.reset_cause() != machine.DEEPSLEEP_RESET) and otaCheck:
        otaAvailable = splash_ota_check()
    else:
        otaAvailable = badge.nvs_get_u8('badge','OTA.ready',0)
    
# Disable WiFi if active
splash_wifi_disable()

# Initialize services
services.setup()

# Initialize timer
splash_timer_init()

# Clean memory
gc.collect()

# Draw homescreen
splash_draw(True, False)
