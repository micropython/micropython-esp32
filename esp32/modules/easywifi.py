# File: easywifi.py
# Version: 1
# Description: Wrapper that makes using wifi simple
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import time, network, badge, easydraw

state = False
failed = False

def status():
    global state
    return state

def failure():
    global failed
    return failed

def force_enable():
    global state
    state = False
    global failed
    failed = False
    enable()

def enable(showStatus=True):
    global failed
    global state
    if not state:
        nw = network.WLAN(network.STA_IF)
        if not nw.isconnected():
            nw.active(True)
            ssid = badge.nvs_get_str('badge', 'wifi.ssid', 'SHA2017-insecure')
            password = badge.nvs_get_str('badge', 'wifi.password')
            if showStatus:
                easydraw.msg("Connecting to '"+ssid+"'...")
            nw.connect(ssid, password) if password else nw.connect(ssid)
            timeout = badge.nvs_get_u8('badge', 'wifi.timeout', 40)
            while not nw.isconnected():
                time.sleep(0.1)
                timeout = timeout - 1
                if (timeout<1):
                    if showStatus:
                        easydraw.msg("Error: could not connect!")
                    disable()
                    failed = True
                    return False
            state = True
            failed = False
            if showStatus:
                easydraw.msg("Connected!")
    return True

def disable():
    global state
    state = False
    global failed
    failed = False
    nw = network.WLAN(network.STA_IF)
    nw.active(False)
