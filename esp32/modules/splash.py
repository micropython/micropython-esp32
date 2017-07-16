import ugfx, time, badge, machine, uos, appglue, deepsleep, network

# SHA2017 badge home screen
# Renze Nicolai 2017

# SERVICES
def setup_services():
    global services
    try:
        apps = uos.listdir('lib')
    except OSError:
        print("[SPLASH] Can't setup services: no lib folder!")
        return False
    for app in apps:
        try:
            files = uos.listdir('lib/'+app)
        except OSError:
            print("[SPLASH] Listing app files for app '"+app+"' failed!")
            return False
        status = True
        found = False
        for f in files:
            if (f=="service.py"):
                found = True
                print("[SPLASH] Running service "+app+"...")
                try:
                    srv = __import__('lib/'+app+'/service')
                    services.append(srv) #Add to global list
                    srv.setup()
                except BaseException as msg:
                    print("Exception in service setup "+app+": ", msg)
                    status = False
                break
        if not found:
            print("[SPLASH] App '"+app+"' has no service")
    return status

def loop_services(loopCnt):
    noSleep = False
    global services
    for srv in services:
        try:
            if (srv.loop(loopCnt)):
                noSleep = True
        except BaseException as msg:
            print("[SPLASH] Service loop exception: ", msg)
    return noSleep
        
# RTC

def clockstring():
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
    return daystr+"-"+monthstr+"-"+str(year)+" "+hourstr+":"+minstr

def disableWifi():
    nw = network.WLAN(network.STA_IF)
    nw.active(False)

def getTimeNTP():
    import ntp
    import wifi
    
    current_datetime = time.time()
    
    if (current_datetime<1482192000): #If clock on time before 2017
        draw_msg("Configuring clock...", "Welcome!")
        wifi.init()
        ssid = badge.nvs_get_str('badge', 'wifi.ssid', 'SHA2017-insecure')
        draw_msg("Configuring clock...", "Connecting to '"+ssid+"'...")
        timeout = 30
        while not wifi.sta_if.isconnected():
            time.sleep(0.1)
            timeout = timeout - 1
            if (timeout<1):
                draw_msg("Error", "Timeout while connecting!")
                disableWifi()
                time.sleep(1)
                return False
            else:
                pass
        draw_msg("Configuring clock...", "NTP...")
        ntp.set_NTP_time()
        draw_msg("Configuring clock...", "Done!")
        disableWifi()
        return True
    #else:
    #    print("[SPLASH] RTC already set.")
    return True
    
# BATTERY

def battery_percent(vempty, vfull, vbatt):
    percent = round(((vbatt-vempty)*100)/(vfull-vempty))
    if (percent<0):
        percent = 0
    if (percent>100):
        percent = 100
    return percent

# GRAPHICS

def draw_msg(title, desc):
    ugfx.clear(ugfx.WHITE)
    ugfx.string(0, 0, title, "PermanentMarker22", ugfx.BLACK)
    ugfx.string(0, 25, desc, "Roboto_Regular12", ugfx.BLACK)
    ugfx.set_lut(ugfx.LUT_FASTER)
    ugfx.flush()
    
def draw_dialog(title, desc, msg):
    ugfx.clear(ugfx.WHITE)
    ugfx.string(0, 0, title, "PermanentMarker22", ugfx.BLACK)
    ugfx.string(0, 25, desc, "Roboto_Regular12", ugfx.BLACK)
    ugfx.string(0, 50, msg, "Roboto_Regular12", ugfx.BLACK)
    ugfx.set_lut(ugfx.LUT_FASTER)
    ugfx.flush()

def draw_logo(x,y,h):
    ugfx.string(x + 20, y,"STILL","Roboto_BlackItalic24",ugfx.BLACK)
    ugfx.string(x, y+25,h,"PermanentMarker22",ugfx.BLACK)
    len = ugfx.get_string_width(h,"PermanentMarker22")
    ugfx.line(x, y + 47, x + 14 + len, y + 47, ugfx.BLACK)
    ugfx.line(x + 10 + len, y + 27, x + 10 + len, y + 45, ugfx.BLACK)
    ugfx.string(x + 10, y + 50,"Anyway","Roboto_BlackItalic24",ugfx.BLACK)
    
def draw_home(percent, cstate, status):
    ugfx.clear(ugfx.WHITE)
    if (cstate):
        ugfx.string(0,  0, str(percent)+"% & Charging... | "+status,"Roboto_Regular12",ugfx.BLACK)
    else:
        ugfx.string(0,  0, str(percent)+"% | "+status,"Roboto_Regular12",ugfx.BLACK)
    
    ugfx.string(0, 14, clockstring(), "Roboto_Regular12",ugfx.BLACK)
    
    htext = badge.nvs_get_str("owner", "htext", "")
    if (htext!=""):
      draw_logo(160, 25, htext)
    
    nick = badge.nvs_get_str("owner", "name", "Anonymous")
    ugfx.string(0, 40, nick, "PermanentMarker36", ugfx.BLACK)
    
    ugfx.set_lut(ugfx.LUT_FULL)
    ugfx.flush()
    ugfx.set_lut(ugfx.LUT_FASTER)
    
def draw_batterylow(percent):
    ugfx.clear(ugfx.WHITE)
    ugfx.string(0, 0, str(percent)+"%  - Battery empty. Please charge me!","Roboto_Regular12",ugfx.BLACK)
    nick = badge.nvs_get_str("owner", "name", ":(           Zzzz...")
    ugfx.string(0, 40, nick, "PermanentMarker36", ugfx.BLACK)
    ugfx.set_lut(ugfx.LUT_FASTER)
    ugfx.flush()
    
# START LAUNCHER
    
def start_launcher(pushed):
    #print("START LAUNCHER: ", pushed)
    if(pushed):
        global splashTimer
        splashTimer.deinit()
        appglue.start_app("launcher")
      
# SLEEP
            
def badge_sleep():
    print("[SPLASH] Going to sleep now...")
    badge.eink_busy_wait() #Always wait for e-ink
    deepsleep.start_sleeping(30000) #Sleep for 30 seconds
    
# TIMER

def splashTimer_callback(tmr):
    global loopCnt
    if loopCnt>9:
        loopCnt = 0
        cstate = badge.battery_charge_status()
        vbatt = badge.battery_volt_sense()
        percent = battery_percent(3800, 4300, vbatt)
        if (cstate) or (percent>95) or (percent<1):
            if (percent==0):
                draw_home(percent, cstate, "Huh?! You removed the battery?")
            else:
                draw_home(percent, cstate, "Press start to open the launcher!")
        else:
            if (percent<10):
                draw_batterylow(percent)
                ugfx.flush()
            else:
                draw_home(percent, cstate, "Zzz...") 
            badge_sleep()
    else:
        if (loop_services(loopCnt)):
            loopCnt = 0
    loopCnt = loopCnt + 1
  
def start_sleep_counter():
    global splashTimer
    splashTimer.init(period=500, mode=machine.Timer.PERIODIC, callback=splashTimer_callback)
  
def stop_sleep_counter():
    global splashTimer
    splashTimer.deinit()
    
def restart_sleep_counter():
    stop_sleep_counter()
    start_sleep_counter()
    
# WELCOME (SETUP, SPONSORS OR CLOCK)

def welcome():
    setupcompleted = int(badge.nvs_get_str('badge', 'setup.state', '0'))
    if (setupcompleted==0): # First boot (open setup)
        print("[SPLASH] Setup not completed. Running setup!")
        appglue.start_app("setup")
    elif (setupcompleted==1): # Second boot (after setup)
        print("[SPLASH] Showing sponsors once...")
        badge.nvs_set_str('badge', 'setup.state', '2') # Only force show sponsors once
        appglue.start_app("sponsors")
    else: # Setup completed
        print("[SPLASH] Normal boot.")
        getTimeNTP() # Set clock if needed

# MAIN
  
def splash_main():   
    cstate = badge.battery_charge_status()
    vbatt = badge.battery_volt_sense()
    percent = battery_percent(3800, 4290, vbatt)
    ugfx.init()
    if (cstate) or (percent>9) or (percent<1):
        ugfx.input_init()
        welcome()
        draw_home(percent, cstate, "Press start to the open launcher!")
        ugfx.input_attach(ugfx.BTN_START, start_launcher)
        global splashTimer
        setup_services()
        start_sleep_counter()
        
    else:
        draw_batterylow(percent)       
        badge_sleep()
  
#Global variables
splashTimer = machine.Timer(0)
services = []
loopCnt = 0

splash_main()
 
