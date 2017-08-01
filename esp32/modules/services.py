# File: services.py
# Version: 3
# API version: 1
# Description: Background services for SHA2017 badge
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>
#          Thomas Roos   <?>

import uos, ujson, easywifi, easyrtc, time, appglue, deepsleep, ugfx, badge, machine

services = [] #List containing all the service objects
loopCallbacks = {} #Dict containing: {<FUNCTION>:<Wifi required on next run>}
drawCallbacks = [] #List containing draw functions

def setup(loopTmr, drawTmr, pmCb=None, drawCb=None):
    global services
    global loopCallbacks
    global drawCallbacks
    
    global loopTimer
    loopTimer = loopTmr
    global drawTimer
    drawTimer = drawTmr
    
    if pmCb:
        print("[SERVICES] Power management callback registered")
        global pmCallback
        pmCallback = pmCb
    
    if drawCb:
        print("[SERVICES] Draw callback registered")
        global drawCallback
        drawCallback = drawCb #This might need a better name...
    
    # Status of wifi
    wifiFailed = False
    
    #Check if lib folder exists and get application list, else stop
    try:
        apps = uos.listdir('lib')
    except OSError:
        return [False, False]
    
    #For each app...
    for app in apps:
        print("APP: "+app)
        try:
            #Try to open and read the json description
            fd = open('/lib/'+app+'/service.json')
            description = ujson.loads(fd.read())
            fd.close()
        except:
            print("[SERVICES] No description found for "+app)
            continue #Or skip the app
        
        print("DESC FOUND FOR: "+app)
        
        try:
            #Try to open the service itself
            fd = open('/lib/'+app+'/service.py')
            fd.close()
        except:
            print("[SERVICES] No script found for "+app)
            continue #Or skip the app
        
        print("SCRIPT FOUND FOR: "+app)
        
        rtcRequired = False # True if RTC should be set before starting service
        loopEnabled = False # True if loop callback is requested
        drawEnabled = False # True if draw callback is requested
        
        wifiInSetup = False # True if wifi needed in setup
        wifiInLoop = False # True if wifi needed in loop
        
        try:
            if description['apiVersion']!=1:
                print("[SERVICES] Service for "+app+" is not compatible with current firmware")
                continue #Skip the app
            wifiInSetup = description['wifi']['setup']
            wifiInLoop = description['wifi']['setup']
            rtcRequired = description['rtc']
            loopEnabled = description['loop']
            drawEnabled = description['draw']
        except:
            print("[SERVICES] Could not parse description of app "+app)
            continue #Skip the app
        
        print("[SERVICES] Found service for "+app)
        
        # Import the service.py script
        try:
            srv = __import__('lib/'+app+'/service')
        except BaseException as msg:
            print("[SERVICES] Could not import service of app "+app+": ", msg)
            continue #Skip the app
        
        if wifiInSetup:
            if wifiFailed:
                print("[SERVICES] Service of app "+app+" requires wifi and wifi failed so the service has been disabled.")
                continue
            if not easywifi.status():
                if not easywifi.enable():
                    wifiFailed = True
                    print("[SERVICES] Could not connect to wifi!")
                    continue # Skip the app

        if rtcRequired and time.time() < 1482192000:
            if not wifiFailed:
                print("[SERVICES] RTC required, configuring...")
                easyrtc.configure()
            else:
                print("[SERVICES] RTC required but not available. Skipping service.")
                continue # Skip the app (because wifi failed and rtc not available)
        
        try:
            srv.setup()
        except BaseException as msg:
            print("[SERVICES] Exception in service setup "+app+": ", msg)
            continue
        
        if loopEnabled:
            try:
                loopCallbacks[srv.loop] = wifiInLoop
            except:
                print("[SERVICES] Loop requested but not defined in service "+app)
            
        if drawEnabled and drawCb:
            try:
                drawCallbacks.append(srv.draw)
            except:
                print("[SERVICES] Draw requested but not defined in service "+app)
        
        # Add the script to the global service list
        services.append(srv)
        
    # Create loop timer
    hasLoopTimer = False
    if len(loopCallbacks)>0:
        print("[SERVICES] There are loop callbacks, starting loop timer!")
        loop_timer_callback(loopTimer)
        hasLoopTimer = True
        
    # Create draw timer
    hasDrawTimer = False
    if len(drawCallbacks)>0 and drawCb:
        print("[SERVICES] There are draw callbacks, starting draw timer!")
        draw_timer_callback(drawTimer)
        hasDrawTimer = True
    return [hasLoopTimer, hasDrawTimer]
            
def loop_timer_callback(tmr):
    global loopCallbacks
    requestedInterval = 99999999
    newLoopCallbacks = loopCallbacks
    for cb in loopCallbacks:
        if loopCallbacks[cb]:
            print("[SERVICES] Loop needs wifi!")
            if not easywifi.status():
                if not easywifi.enable():
                    print("[SERVICES] Wifi not available!")
                    continue
        else:
            print("[SERVICES] Loop does not need wifi!")
        rqi = 0
        try:
            rqi = cb()
        except BaseException as msg:
            print("[SERVICES] Exception in service loop: ", msg)
            newLoopCallbacks.pop(cb)
            continue
        if rqi>0 and rqi<requestedInterval:
            # Service wants to loop again in rqi ms
            requestedInterval = rqi
        elif rqi<=0:
            # Service doesn't want to loop again until next wakeup
            newLoopCallbacks.pop(cb)
    loopCallbacks = newLoopCallbacks
    del(newLoopCallbacks)
    
    if requestedInterval>=99999999:
        print("[SERVICES] No loop interval returned.")
        requestedInterval = -1
        
    if requestedInterval<1000:
        requestedInterval = 1000
        
    easywifi.disable() # Always disable wifi
    
    try:
        global pmCallback
        if pmCallback(requestedInterval):
            print("[SERVICES] Loop timer (re-)started "+str(requestedInterval))
            tmr.deinit()
            try:
                tmrperiod = round(requestedInterval/1000)*1000
                tmr.init(period=tmrperiod, mode=machine.Timer.ONE_SHOT, callback=loop_timer_callback)
            except BaseException as msg:
                print("TIMER INIT ERROR: LOOP TIMER - ", msg)
    except:
        print("[SERVICES] Error in power management callback!")

def draw_timer_callback(tmr):
    global drawCallback #The function that allows us to hook into our host
    global drawCallbacks #The functions of the services
    requestedInterval = 99999999
    y = ugfx.height()
    
    drawCallback(False) # Prepare draw
    
    newDrawCallbacks = drawCallbacks
    for i in range(0, len(drawCallbacks)):
        cb = drawCallbacks[i]
        rqi = 0
        try:
            [rqi, space_used] = cb(y)
            y = y - space_used
        except BaseException as msg:
            print("[SERVICES] Exception in service draw: ", msg)
            newDrawCallbacks.pop(i)
            continue
        if rqi>0 and rqi<requestedInterval:
            # Service wants to loop again in rqi ms
            requestedInterval = rqi
        elif rqi<=0:
            # Service doesn't want to draw again until next wakeup
            newDrawCallbacks.pop(cb)
    drawCallbacks = newDrawCallbacks
    del(newDrawCallbacks)
    
    badge.eink_busy_wait()
    
    if requestedInterval>=99999999:
        print("[SERVICES] No draw interval returned.")
        requestedInterval = -1
        
    if requestedInterval<1000:
        requestedInterval = 1000
    
    if len(drawCallbacks)>0 and requestedInterval>=0:
        print("[SERVICES] New draw requested in "+str(requestedInterval))
        tmr.deinit()
        try:
            tmrperiod = round(requestedInterval/1000)*1000
            tmr.init(period=tmrperiod, mode=machine.Timer.ONE_SHOT, callback=draw_timer_callback) 
        except BaseException as msg:
            print("TIMER INIT ERROR: DRAW TIMER - ",msg)
    drawCallback(True) # Complete draw

def force_draw(disableTimer):
    if disableTimer:
        print("[SERVICES] Drawing services one last time before sleep...")
        global drawTimer
        try:
            drawTimer.deinit()
        except:
            pass
    else:
        print("[SERVICES] Drawing at boot...")
    global drawCallbacks
    if len(drawCallbacks)>0:
        y = ugfx.height()
        for cb in drawCallbacks:
            try:
                [rqi, space_used] = cb(y)
                y = y - space_used
            except BaseException as msg:
                print("[SERVICES] Exception in service draw: ", msg)
