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

def draw_services():
    noSleep = False
    global services
    x = 0
    y = ugfx.height() - 14
    for srv in services:
        try:
            space_used = srv.draw(x,y)
            if (space_used>0):
                y = y - abs(space_used)
        except BaseException as msg:
            print("[SPLASH] Service draw exception: ", msg)
        
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

def battery_percent_internal(vempty, vfull, vbatt):
    percent = round(((vbatt-vempty)*100)/(vfull-vempty))
    if (percent<0):
        percent = 0
    if (percent>100):
        percent = 100
    return percent

def battery_percent():
        vbatt = 0
        for i in range(0,10):
            vbatt = vbatt + badge.battery_volt_sense()
        percent = battery_percent_internal(3800, 4300, round(vbatt/10))
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
    
def draw_helper_clear(full):
    if full:
        ugfx.clear(ugfx.BLACK)
        ugfx.flush()
    ugfx.clear(ugfx.WHITE)
    if full:
        ugfx.flush()
    
def draw_helper_battery(percent,cstate):
    ugfx.area(2,2,40,18,ugfx.WHITE)
    ugfx.box(42,7,2,8,ugfx.WHITE)
    if (percent>0):
        if (cstate):
            ugfx.string(5,5,"chrg","Roboto_Regular12",ugfx.BLACK)
        else:
            if (percent>10):
                w = round((percent*38)/100)
                ugfx.area(3,3,w,16,ugfx.BLACK)
            else:
                ugfx.string(5,5,"empty","Roboto_Regular12",ugfx.BLACK)
    else:
        ugfx.string(2,5,"no batt","Roboto_Regular12",ugfx.BLACK)
    
def draw_helper_header(text):
    ugfx.area(0,0,ugfx.width(),23,ugfx.BLACK)
    ugfx.string(45, 1, text,"DejaVuSans20",ugfx.WHITE)
    
def draw_helper_footer(text_l, text_r):
    ugfx.string(0, ugfx.height()-13, text_l, "Roboto_Regular12",ugfx.BLACK)
    l = ugfx.get_string_width(text_r,"Roboto_Regular12")
    ugfx.string(ugfx.width()-l, ugfx.height()-13, text_r, "Roboto_Regular12",ugfx.BLACK)
    
def draw_helper_nick(default):
    nick = badge.nvs_get_str("owner", "name", default)
    ugfx.string(0, 40, nick, "PermanentMarker36", ugfx.BLACK)
    htext = badge.nvs_get_str("owner", "htext", "")
    if (htext!=""):
      draw_logo(160, 25, htext)
    
def draw_helper_flush(full):
    if (full):
        ugfx.set_lut(ugfx.LUT_FULL)
    ugfx.flush()
    ugfx.set_lut(ugfx.LUT_FASTER)
    
def draw_home(percent, cstate, status, full_clear, going_to_sleep):
    draw_helper_clear(full_clear)
    if (cstate):
        draw_helper_header(status)
    else:
        draw_helper_header(status)
    draw_helper_battery(percent, cstate)
    if (going_to_sleep):
        info = "[ ANY: Wake up ]"
    else:
        info = "[ START: LAUNCHER ]"
    draw_helper_footer(clockstring(),info)
    draw_helper_nick("Unknown")
    draw_services()
    draw_helper_flush(True)
    
def draw_batterylow(percent):
    draw_helper_clear(True)
    draw_helper_header("")
    draw_helper_battery(percent, False)
    draw_helper_footer("BATTERY LOW, CONNECT CHARGER!", "")
    draw_helper_nick(":(           Zzzz...")
    draw_helper_flush(False)
    
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
    
def badge_sleep_forever():
    print("[SPLASH] Going to sleep WITHOUT TIME WAKEUP now...")
    badge.eink_busy_wait() #Always wait for e-ink
    deepsleep.start_sleeping(0) #Sleep until button interrupt occurs
    
# TIMER

def splashTimer_callback(tmr):
    global loopCnt
    if loopCnt>9:
        loopCnt = 0
        cstate = badge.battery_charge_status()
        percent = battery_percent()
        vbatt = badge.battery_volt_sense()
        if (cstate) or (percent>95) or (vbatt<100):
            draw_home(percent, cstate, "", False, False)
        else:
            if (percent<10):
                draw_batterylow(percent)
                ugfx.flush()
                badge_sleep_forever()
            else:
                draw_home(percent, cstate, "Zzz...", True, True)
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
    percent = battery_percent()
    vbatt = badge.battery_volt_sense()
    print("[SPLASH] Vbatt = "+str(vbatt))
    ugfx.init()
    if (cstate) or (percent>9) or (vbatt<100):
        ugfx.input_init()
        welcome()
        draw_home(percent, cstate, "", True, False)
        ugfx.input_attach(ugfx.BTN_START, start_launcher)
        global splashTimer
        setup_services()
        start_sleep_counter()
    else:
        draw_batterylow(percent)       
        badge_sleep_forever()
  
#Global variables
splashTimer = machine.Timer(0)
services = []
loopCnt = 0

splash_main()
 
