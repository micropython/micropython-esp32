# File: services.py
# Version: 4
# API version: 2
# Description: Background services for SHA2017 badge
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>
#          Thomas Roos   <?>

import uos, ujson, easywifi, easyrtc, time, appglue, deepsleep, ugfx, badge, machine, sys, virtualtimers

services = [] #List containing all the service objects
drawCallbacks = [] #List containing draw functions

def setup(drawCb=None):
    global services
    global drawCallbacks
        
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
                
        try:
            #Try to open the service itself
            fd = open('/lib/'+app+'/service.py')
            fd.close()
        except:
            print("[SERVICES] No script found for "+app)
            continue #Or skip the app
                
        rtcRequired = False # True if RTC should be set before starting service
        loopEnabled = False # True if loop callback is requested
        drawEnabled = False # True if draw callback is requested
        
        wifiInSetup = False # True if wifi needed in setup
        wifiInLoop = False # True if wifi needed in loop
        
        try:
            if description['apiVersion']!=2:
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
        except BaseException as e:
            print("[SERVICES] Could not import service of app "+app+": ")
            sys.print_exception(e)
            continue #Skip the app
        
        if wifiInSetup or wifiInLoop:
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
        except BaseException as e:
            print("[SERVICES] Exception in service setup "+app+":")
            sys.print_exception(e)
            continue
        
        if loopEnabled:
            try:
                virtualtimers.new(1, srv.loop)
            except:
                print("[SERVICES] Loop requested but not defined in service "+app)
            
        if drawEnabled and drawCb:
            try:
                drawCallbacks.append(srv.draw)
            except:
                print("[SERVICES] Draw requested but not defined in service "+app)
        
        # Add the script to the global service list
        services.append(srv)

    handleDraw = False
    if len(drawCallbacks)>0 and drawCb:
        print("[SERVICES] The service subsystem now handles screen redraws")
        handleDraw = True
        virtualtimers.new(1, draw_task, True)
    return handleDraw

def draw_task():
    global drawCallback #The function that allows us to hook into our host
    global drawCallbacks #The functions of the services
    requestedInterval = 99999999
    y = ugfx.height()
    
    drawCallback(False) # Prepare draw
    
    deleted = []

    for i in range(0, len(drawCallbacks)):
        cb = drawCallbacks[i]
        rqi = 0
        try:
            [rqi, space_used] = cb(y)
            y = y - space_used
        except BaseException as e:
            print("[SERVICES] Exception in service draw:")
            sys.print_exception(e)
            deleted.append(cb)
            continue
        if rqi>0 and rqi<requestedInterval:
            # Service wants to loop again in rqi ms
            requestedInterval = rqi
        elif rqi<=0:
            # Service doesn't want to draw again until next wakeup
            deleted.append(cb)
    
    for i in range(0,len(deleted)):
        print("[DEBUG] Deleted draw callback: ",dcb)
        drawCallbacks = list(dcb for dcb in drawCallbacks if dcb!=deleted[i])
    
    badge.eink_busy_wait()
    
    if requestedInterval>=99999999:
        print("[SERVICES] No draw interval returned.")
        requestedInterval = -1
        
    if requestedInterval<1000:
        #Draw at most once a second
        print("[SERVICES] Can't draw more than once a second!")
        requestedInterval = 1000
    
    retVal = 0
    
    if len(drawCallbacks)>0 and requestedInterval>=0:
        print("[SERVICES] New draw requested in "+str(requestedInterval))
        retVal = requestedInterval
    drawCallback(True) # Complete draw
    return retVal

def force_draw():
    global drawCallbacks
    if len(drawCallbacks)>0:
        y = ugfx.height()
        for cb in drawCallbacks:
            try:
                [rqi, space_used] = cb(y)
                y = y - space_used
            except BaseException as e:
                print("[SERVICES] Exception in service draw: ")
                sys.print_exception(e)
