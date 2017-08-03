# File: splash.py
# Version: 5
# Description: Homescreen for SHA2017 badge
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>
#          Thomas Roos   <?>

import ugfx, time, badge, machine, deepsleep, gc
import appglue, virtualtimers
import easydraw, easywifi, easyrtc

import tasks.powermanagement as pm
import tasks.otacheck as otac
import tasks.resourcescheck as resc
import tasks.sponsorscheck as spoc
import tasks.services as services
import tasks.badgeeventreminder as ber

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
            if otac.available(False):
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
        appglue.start_app("launcher", False)

def splash_input_a(pressed):
    if pressed:
        splash_about_countdown_trigger()
        pm.feed()

def splash_input_select(pressed):
    if pressed:
        if otac.available(False):
            appglue.start_ota()
        pm.feed()

def splash_input_other(pressed):
    if pressed:
        pm.feed()

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

# Power management
 
def onSleep(idleTime):
    draw(False, True)
    services.force_draw()
    draw(True, True)

### PROGRAM

# Calibrate battery voltage drop
if badge.battery_charge_status() == False and badge.usb_volt_sense() > 4500 and badge.battery_volt_sense() > 2500:
    badge.nvs_set_u16('splash', 'bat.volt.drop', 5200 - badge.battery_volt_sense()) # mV
    print('Set vDrop to: ' + str(4200 - badge.battery_volt_sense()))
vDrop = badge.nvs_get_u16('splash', 'bat.volt.drop', 1000) - 1000 # mV

splash_input_init()
splash_about_countdown_reset()

# post ota script
import post_ota

setupState = badge.nvs_get_u8('badge', 'setup.state', 0)
if setupState == 0: #First boot
    print("[SPLASH] First boot (start setup)...")
    appglue.start_app("setup")
elif setupState == 1: # Second boot: Show sponsors
    print("[SPLASH] Second boot (show sponsors)...")
    badge.nvs_set_u8('badge', 'setup.state', 2)
    spoc.show(True)
elif setupState == 2: # Third boot: force OTA check
    print("[SPLASH] Third boot (force ota check)...")
    badge.nvs_set_u8('badge', 'setup.state', 3)
    if not easywifi.failure():
        otac.available(True)
else: # Normal boot
    print("[SPLASH] Normal boot... ("+str(machine.reset_cause())+")")
    if (machine.reset_cause() != machine.DEEPSLEEP_RESET):
        print("... from reset: checking for ota update")
        if not easywifi.failure():
            otac.available(True)

# RTC ===
if time.time() < 1482192000:
    easyrtc.configure()
# =======
        
if not easywifi.failure():
    resc.check()        # Check resources
if not easywifi.failure():
    spoc.show(False)    # Check sponsors

services.setup(draw) # Start services

draw(False)
services.force_draw()
draw(True)

easywifi.disable()
gc.collect()
    
virtualtimers.debug(True)
virtualtimers.activate(25)
pm.callback(onSleep)
pm.feed()

ber.enable()

print("----")
print("WARNING: POWER MANAGEMENT ACTIVE")
print("TO USE REPL START LAUNCHER")
print("----")
