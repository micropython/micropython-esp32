# File: splash.py
# Version: 4
# Description: Homescreen for SHA2017 badge
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>
#          Thomas Roos   <?>

import ugfx, time, ntp, badge, machine, deepsleep, network, esp, gc
import appglue, services

import easydraw, easywifi, easyrtc

### FUNCTIONS

# Graphics
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
            info2 = easyrtc.string(True, True)

    l = ugfx.get_string_width(info1,"Roboto_Regular12")
    ugfx.string(296-l, 0, info1, "Roboto_Regular12",ugfx.BLACK)
    l = ugfx.get_string_width(info2,"Roboto_Regular12")
    ugfx.string(296-l, 12, info2, "Roboto_Regular12",ugfx.BLACK)

def splash_draw(full=False,sleeping=False):    
    if easydraw.msgShown:
        easydraw.msgShown = False
        easydraw.msgLineNumber = 0
        full = True
        
    vUsb = badge.usb_volt_sense()
    vBatt = badge.battery_volt_sense()
    vBatt += vDrop
    charging = badge.battery_charge_status()
        
    if splash_power_countdown_get()<1:
        full= True
    
    if full:
        ugfx.clear(ugfx.WHITE)
        easydraw.nickname()
    else:
        ugfx.area(0,0,ugfx.width(),24,ugfx.WHITE)
        ugfx.area(0,ugfx.height()-64,ugfx.width(),64,ugfx.WHITE)
    
    easydraw.battery(vUsb, vBatt, charging)    
    
    global splashPowerCountdown
    
    if splashPowerCountdown>0:
        if vBatt>500:
            ugfx.string(52, 0, str(round(vBatt/1000, 1)) + 'v','Roboto_Regular12',ugfx.BLACK)
        if splashPowerCountdown>0 and splashPowerCountdown<badge.nvs_get_u8('splash', 'timer.amount', 30):
            ugfx.string(52, 13, "Sleeping in "+str(splashPowerCountdown)+"...",'Roboto_Regular12',ugfx.BLACK)
    
    splash_draw_actions(sleeping)
    
    services.draw()
    
    if full:
        ugfx.flush(ugfx.LUT_FULL)
    else:
        ugfx.flush(ugfx.LUT_NORMAL)

# OTA update checking

def splash_ota_download_info():
    import urequests as requests
    easydraw.msg("Checking for updates...", True)
    result = False
    try:
        data = requests.get("https://badge.sha2017.org/version")
    except:
        easydraw.msg("Error:")
        easydraw.msg("Could not download JSON!")
        time.sleep(5)
        return False
    try:
        result = data.json()
    except:
        data.close()
        easydraw.msg("Error:")
        easydraw.msg("Could not decode JSON!")
        time.sleep(5)
        return False
    data.close()
    return result

def splash_ota_check():
    if not easywifi.status():
        if not easywifi.enable():
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
    
# Resources
def splash_resources_install():
    easydraw.msg("Installing resources...",True)
    if not easywifi.status():
        if not easywifi.enable():
            return False
    import woezel
    woezel.install("resources")
    appglue.home()

def splash_resources_check():
    needToInstall = True
    try:
        fp = open("/lib/resources/version", "r")
        version = int(fp.read(99))
        print("[SPLASH] Current resources version: "+str(version))
        if version>=2:
            needToInstall = False
    except:
        pass
    if needToInstall:
        print("[SPLASH] Resources need to be updated!")
        splash_resources_install()
        return True
    return False

# Sponsors

def splash_sponsors_install():
    if not easywifi.status():
        if not easywifi.enable():
            return False
    print("[SPLASH] Installing sponsors...")
    easydraw.msg("Installing sponsors...",True)
    import woezel
    woezel.install("sponsors")
    easydraw.msg("Done.")

def splash_sponsors_show():
    needToInstall = True
    version = 0
    try:
        fp = open("/lib/sponsors/version", "r")
        version = int(fp.read(99))
        print("[SPLASH] Current sponsors version: "+str(version))
    except:
        print("[SPLASH] Sponsors not installed.")
    if version>=14:
        needToInstall = False 
    if needToInstall:
        splash_sponsors_install()
    try:
        fp = open("/lib/sponsors/version", "r")
        version = int(fp.read(99))
        # Now we know for sure that a version of the sponsors app has been installed
        badge.nvs_set_u8('sponsors', 'shown', 1)
        appglue.start_app("sponsors")
    except:
        pass
    

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
otaAvailable = badge.nvs_get_u8('badge','OTA.ready',0)

# Calibrate battery voltage drop
if badge.battery_charge_status() == False and badge.usb_volt_sense() > 4500 and badge.battery_volt_sense() > 2500:
    badge.nvs_set_u16('splash', 'bat.volt.drop', 5200 - badge.battery_volt_sense()) # mV
    print('Set vDrop to: ' + str(4200 - badge.battery_volt_sense()))
vDrop = badge.nvs_get_u16('splash', 'bat.volt.drop', 1000) - 1000 # mV

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
    splash_sponsors_show()
elif setupState == 2: # Third boot: force OTA check
    print("[SPLASH] Third boot...")
    badge.nvs_set_u8('badge', 'setup.state', 3)
    otaCheck = easyrtc.configure() if time.time() < 1482192000 else True
    otaAvailable = splash_ota_check()
else: # Normal boot
    print("[SPLASH] Normal boot...")
    otaCheck = easyrtc.configure() if time.time() < 1482192000 else True
    if (machine.reset_cause() != machine.DEEPSLEEP_RESET) and otaCheck:
        otaAvailable = splash_ota_check()
    else:
        otaAvailable = badge.nvs_get_u8('badge','OTA.ready',0)
    
# Download resources to fatfs
splash_resources_check()

# Show updated sponsors if not yet shown
if badge.nvs_get_u8('sponsors', 'shown', 0)<1:
    splash_sponsors_show()

# Initialize services
services.setup()
    
# Disable WiFi if active
easywifi.disable()

# Initialize timer
splash_timer_init()

# Clean memory
gc.collect()

# Draw homescreen
splash_draw(True, False)
