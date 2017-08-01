# File: splash.py
# Version: 4
# Description: Homescreen for SHA2017 badge
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>
#          Thomas Roos   <?>

import ugfx, time, ntp, badge, machine, deepsleep, network, esp, gc
import appglue, services

import easydraw, easywifi, easyrtc

# Power management

enableBpp = True
requestedStandbyTime = 0

def power_management(timeUntilNextTick):
    global requestedStandbyTime
    requestedStandbyTime = timeUntilNextTick
    if timeUntilNextTick<0 or timeUntilNextTick>=60*5000:
        print("[PM] Next tick after more than 5 minutes (can do bpp).")
        global enableBpp
        enableBpp = True
        power_countdown_reset(0)
    elif (timeUntilNextTick<=30000):
        print("[PM] Next loop in "+str(timeUntilNextTick)+" ms (stay awake).")
        power_countdown_reset(timeUntilNextTick)
        return True #Service loop timer can be restarted
    else:
        print("[PM] Next loop in "+str(timeUntilNextTick)+" ms (can sleep).")
        global enableBpp
        enableBpp = False
        power_countdown_reset(0)
    return False

powerCountdownOffset = 0
def power_countdown_reset(timeUntilNextTick=-1):
    global powerTimer
    global powerCountdownOffset
    
    global scheduler
    found = False
    for i in range(0, len(scheduler)):
        if (scheduler[i]["cb"]==power_countdown_callback):
            scheduler[i]["pos"] = 0
            scheduler[i]["target"] = powerCountdownOffset+badge.nvs_get_u16('splash', 'urt', 5000)
            found = True
            break
    if not found:
        scheduler_add(powerCountdownOffset+badge.nvs_get_u16('splash', 'urt', 5000), power_countdown_callback)
    
def power_countdown_callback():
    global enableBpp
    draw(False, True)
    services.force_draw(True)
    draw(True, True)
    global requestedStandbyTime
    if requestedStandbyTime>0:
            print("[PM] Sleep for "+str(requestedStandbyTime)+" ms.")
            deepsleep.start_sleeping(requestedStandbyTime)
    else:
            print("[PM] BPP forever.")
            deepsleep.start_sleeping()

# Graphics

def draw(mode, goingToSleep=False):
    if mode:
        # We flush the buffer and wait
        ugfx.flush(ugfx.LUT_FULL)
        badge.eink_busy_wait()
    else:
        # We prepare the screen refresh
        ugfx.clear(ugfx.WHITE)
        if goingToSleep:
            info1 = 'Sleeping...'
            info2 = 'Press any key to wake up'
        else:
            info1 = 'Press start to open the launcher'
            global otaAvailable
            if otaAvailable:
                info2 = 'Press select to start OTA update'
            else:
                info2 = ''
        l = ugfx.get_string_width(info1,"Roboto_Regular12")
        ugfx.string(296-l, 0, info1, "Roboto_Regular12",ugfx.BLACK)
        l = ugfx.get_string_width(info2,"Roboto_Regular12")
        ugfx.string(296-l, 12, info2, "Roboto_Regular12",ugfx.BLACK)
        
        easydraw.nickname()
        
        vUsb = badge.usb_volt_sense()
        vBatt = badge.battery_volt_sense()
        vBatt += vDrop
        charging = badge.battery_charge_status()

        easydraw.battery(vUsb, vBatt, charging)
        
        if vBatt>500:
            ugfx.string(52, 0, str(round(vBatt/1000, 1)) + 'v','Roboto_Regular12',ugfx.BLACK)

# OTA update checking

def splash_ota_download_info():
    import urequests as requests
    easydraw.msg("Checking for updates...")
    result = False
    try:
        data = requests.get("https://badge.sha2017.org/version")
    except:
        easydraw.msg("Error: could not download JSON!")
        time.sleep(5)
        return False
    try:
        result = data.json()
    except:
        data.close()
        easydraw.msg("Error: could not decode JSON!")
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
    easydraw.msg("Installing resources...")
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
    easydraw.msg("Installing sponsors...")
    import woezel
    woezel.install("sponsors")
    easydraw.msg("Done!")

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

# Button input

def splash_input_start(pressed):
    # Pressing start always starts the launcher
    if pressed:
        print("[SPLASH] Start button pressed")
        appglue.start_app("launcher", False)

def splash_input_a(pressed):
    if pressed:
        print("[SPLASH] A button pressed")
        splash_about_countdown_trigger()
        power_countdown_reset()

def splash_input_select(pressed):
    if pressed:
        print("[SPLASH] Select button pressed")
        global otaAvailable
        if otaAvailable:
            splash_ota_start()
        power_countdown_reset()

#def splash_input_left(pressed):
#    if pressed:
#        appglue.start_bpp()

def splash_input_other(pressed):
    if pressed:
        print("[SPLASH] Other button pressed")
        power_countdown_reset()

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
    

# TIMERS

scheduler = []

loopTimerPos = 0
loopTimerTarget = 0
drawTimerPos = 0
drawTimerTarget = 0
powerTimerPos = 0
powerTimerTarget = 0

def scheduler_add(target, callback):
    global scheduler
    item = {"pos":0, "target":target, "cb":callback}
    scheduler.append(item)

def timer_callback(tmr):
    global scheduler
    newScheduler = scheduler
    for i in range(0, len(scheduler)):
        scheduler[i]["pos"] += 25
        if scheduler[i]["pos"] > scheduler[i]["target"]:
            print("Target reached "+str(i))
            newTarget = scheduler[i]["cb"]()
            if newTarget > 0:
                print("New target "+str(i))
                newScheduler[i]["pos"] = 0
                newScheduler[i]["target"] = newTarget
            else:
                print("Discard "+str(i))
                newScheduler[i]["pos"] = -1
                newScheduler[i]["target"] = -1
        
### PROGRAM

timer = machine.Timer(0)
timer.init(period=25, mode=machine.Timer.PERIODIC, callback=timer_callback)

# Load settings from NVS
otaAvailable = badge.nvs_get_u8('badge','OTA.ready',0)

# Calibrate battery voltage drop
if badge.battery_charge_status() == False and badge.usb_volt_sense() > 4500 and badge.battery_volt_sense() > 2500:
    badge.nvs_set_u16('splash', 'bat.volt.drop', 5200 - badge.battery_volt_sense()) # mV
    print('Set vDrop to: ' + str(4200 - badge.battery_volt_sense()))
vDrop = badge.nvs_get_u16('splash', 'bat.volt.drop', 1000) - 1000 # mV

# Initialize user input subsystem
splash_input_init()

# Initialize about subsystem
splash_about_countdown_reset()

# Setup / Sponsors / OTA check / NTP clock sync
setupState = badge.nvs_get_u8('badge', 'setup.state', 0)
if setupState == 0: #First boot
    print("[SPLASH] First boot (start setup)...")
    appglue.start_app("setup")
elif setupState == 1: # Second boot: Show sponsors
    print("[SPLASH] Second boot (show sponsors)...")
    badge.nvs_set_u8('badge', 'setup.state', 2)
    splash_sponsors_show()
elif setupState == 2: # Third boot: force OTA check
    print("[SPLASH] Third boot (force ota check)...")
    badge.nvs_set_u8('badge', 'setup.state', 3)
    otaAvailable = splash_ota_check()
else: # Normal boot
    print("[SPLASH] Normal boot... ")
    print("RESET CAUSE: "+str(machine.reset_cause()))
    if (machine.reset_cause() != machine.DEEPSLEEP_RESET):
        print("... from reset: checking for ota update")
        otaAvailable = splash_ota_check()
    else:
        print("... from deep sleep: loading ota state from nvs")
        otaAvailable = badge.nvs_get_u8('badge','OTA.ready',0)

# Download resources to fatfs
splash_resources_check()

# Show updated sponsors if not yet shown
if badge.nvs_get_u8('sponsors', 'shown', 0)<1:
    splash_sponsors_show()

# Initialize services
print("Initialize services")
[srvDoesLoop, srvDoesDraw] = services.setup(power_management, draw)

if srvDoesLoop:
    print("Service does loop.")
    requestedInterval = services.loop_timer()
    if requestedInterval>0:
        scheduler_add(requestedInterval, services.loop_timer)
        #loopTimer.init(period=requestedInterval, mode=machine.Timer.ONE_SHOT, callback=loop_timer_callback)
    
if srvDoesDraw:
    print("Service does draw.")
    requestedInterval = services.draw_timer()
    if requestedInterval>0:
        scheduler_add(requestedInterval, services.draw_timer)
        #drawTimer.init(period=requestedInterval, mode=machine.Timer.ONE_SHOT, callback=draw_timer_callback) 

# Disable WiFi if active
print("Disable WiFi if active")
easywifi.disable()

# Clean memory
print("Clean memory")
gc.collect()

# Draw homescreen
print("Draw homescreen")
if not srvDoesDraw:
    print("[SPLASH] No service draw, manually drawing...")
    draw(False)
    draw(True)

if not srvDoesLoop:
    print("[SPLASH] No service sleep!")

power_countdown_reset(-1)

print("----")
print("WARNING: NOT IN REPL MODE, POWER MANAGEMENT ACTIVE")
print("TO USE REPL RESET AND HIT CTRL+C BEFORE SPLASH STARTS")
print("----")
