import ugfx, time, badge, machine, uos, appglue, deepsleep, network, esp

# SHA2017 badge home screen
# Renze Nicolai 2017

# BPP
def get_reboot_counter():
    return esp.rtcmem_read(1023)

def set_reboot_counter(value):
    esp.rtcmem_write(1023, value)
    
def increment_reboot_counter():
    val = get_reboot_counter()
    if (val<255):
        val = val + 1
    set_reboot_counter(val)
    
def bpp_execute():
    print("[SPLASH] Executing BPP...")
    set_reboot_counter(0)
    esp.rtcmem_write(0,2)
    esp.rtcmem_write(0,~2)
    deepsleep.reboot()
    
def bpp_check():
    global bpp_after_count
    if (get_reboot_counter()>bpp_after_count):
        return True
    return False

# SERVICES
def setup_services():
    global services
    try:
        apps = uos.listdir('lib')
    except OSError:
        print("[SPLASH] Can't setup services: no lib folder!")
        return False
    status = True #True if no error occured
    for app in apps:
        try:
            files = uos.listdir('lib/'+app)
        except OSError:
            print("[SPLASH] Listing app files for app '"+app+"' failed!")
            return False
        
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
                    status = False #Error: status is now False
                break
        if not found:
            print("[SPLASH] App '"+app+"' has no service")
    return status

def loop_services(lcnt):
    noSleep = False
    global services
    for srv in services:
        try:
            if (srv.loop(lcnt)):
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
        global ntp_timeout
        timeout = ntp_timeout
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
            global battery_volt_min
            global battery_volt_max
        percent = battery_percent_internal(battery_volt_min, battery_volt_max, round(vbatt/10))
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
    global header_fg
    global header_bg
    ugfx.area(2,2,40,18,header_fg)
    ugfx.box(42,7,2,8,header_fg)
    if (percent>0):
        if (cstate):
            ugfx.string(5,5,"chrg","Roboto_Regular12",header_bg)
        else:
            if (percent>10):
                w = round((percent*38)/100)
                ugfx.area(3,3,38,16,ugfx.WHITE)
                ugfx.area(3,3,w,16,ugfx.BLACK)
            else:
                ugfx.string(5,5,"empty","Roboto_Regular12",header_bg)
    else:
        ugfx.string(2,5,"no batt","Roboto_Regular12",header_bg)
    
def draw_helper_header(text):
    global header_fg
    global header_bg
    ugfx.area(0,0,ugfx.width(),23,header_bg)
    ugfx.string(45, 1, text,"DejaVuSans20",header_fg)
    
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
    global header_hide_while_sleeping
    if (header_hide_while_sleeping and going_to_sleep):
        print("[SPLASH] Hiding header while sleeping")
    else:
        draw_helper_header(status)
        global header_hide_battery_while_sleeping
        if (header_hide_battery_while_sleeping and going_to_sleep):
            print("[SPLASH] Hiding battery while sleeping")
        else:
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
    if(pushed):
        global splashTimer
        splashTimer.deinit()
        increment_reboot_counter()
        appglue.start_app("launcher")
      
# SLEEP
            
def badge_sleep():
    increment_reboot_counter()
    global sleep_duration
    print("[SPLASH] Going to sleep now...")
    badge.eink_busy_wait() #Always wait for e-ink
    deepsleep.start_sleeping(sleep_duration)
    
def badge_sleep_forever():
    increment_reboot_counter()
    print("[SPLASH] Going to sleep WITHOUT TIME WAKEUP now...")
    badge.eink_busy_wait() #Always wait for e-ink
    deepsleep.start_sleeping(0) #Sleep until button interrupt occurs
    
# TIMER

def splashTimer_callback(tmr):
    global loopCnt
    global timer_loop_amount
    if loopCnt<1:
        loopCnt = timer_loop_amount
        cstate = badge.battery_charge_status()
        percent = battery_percent()
        vbatt = badge.battery_volt_sense()
        if (cstate) or (percent>98) or (vbatt<100): #If charging, charged or without battery
            global header_status_string
            draw_home(percent, cstate, header_status_string, False, False) #Update display
        else:
            global battery_percent_empty
            if (percent<=battery_percent_empty):
                draw_batterylow(percent)
                ugfx.flush()
                badge_sleep_forever()
            else:
                if (bpp_check()):
                    draw_home(percent, cstate, "BPP", False, True)
                    bpp_execute()
                else:
                    draw_home(percent, cstate, "Zzz...", False, True)
                    badge_sleep()
    else:
        if (loop_services(loopCnt)):
            loopCnt = timer_loop_amount
    loopCnt = loopCnt - 1
  
def start_sleep_counter():
    global splashTimer
    global splash_timer_interval
    splashTimer.init(period=splash_timer_interval, mode=machine.Timer.PERIODIC, callback=splashTimer_callback)
  
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

# SETTINGS FROM NVS

def load_settings():
    header_inv = int(badge.nvs_get_str('splash', 'header.invert', '0'))
    if (header_inv>0):
        global header_fg
        global header_bg
        header_fg = ugfx.WHITE
        header_bg = ugfx.BLACK
    header_hws = int(badge.nvs_get_str('splash', 'header.hws', '0')) #Hide While Sleeping
    if (header_hws>0):
        global header_hide_while_sleeping
        header_hide_while_sleeping = True
    header_hbws = int(badge.nvs_get_str('splash', 'header.hbws', '0')) #Hide Battery While Sleeping
    if (header_hbws>0):
        global header_hide_battery_while_sleeping
        header_hide_battery_while_sleeping = True
    global sleep_duration
    sleep_duration = int(badge.nvs_get_str('splash', 'sleep.duration', '60000'))
    if (sleep_duration<30000):
        print("[SPLASH] Sleep duration set to less than 30 seconds. Forcing 30 seconds.")
        sleep_duration = 30000
    if (sleep_duration>120000):
        print("[SPLASH] Sleep duration set to more than 120 seconds. Forcing 120 seconds.") 
    global battery_volt_min
    battery_volt_min = int(badge.nvs_get_str('splash', 'battery.volt.min', '3800')) # volt
    global battery_volt_max
    battery_volt_max = int(badge.nvs_get_str('splash', 'battery.volt.max', '4300')) # volt
    global battery_percent_empty
    battery_percent_empty = int(badge.nvs_get_str('splash', 'battery.percent.empty', '1')) # %
    global ntp_timeout
    ntp_timeout = int(badge.nvs_get_str('splash', 'ntp.timeout', '40')) #amount of tries
    global bpp_after_count
    bpp_after_count = int(badge.nvs_get_str('splash', 'bpp.count', '5'))
    global splash_timer_interval
    splash_timer_interval = int(badge.nvs_get_str('splash', 'timer.interval', '500'))
    global timer_loop_amount
    timer_loop_amount = int(badge.nvs_get_str('splash', 'timer.amount', '10'))
    
# MAIN
  
def splash_main():   
    cstate = badge.battery_charge_status()
    percent = battery_percent()
    vbatt = badge.battery_volt_sense()
    print("[SPLASH] Vbatt = "+str(vbatt))
    load_settings()
    
    global header_status_string
    header_status_string = str(get_reboot_counter())
    
    ugfx.init()
    global battery_percent_empty
    if (cstate) or (percent>battery_percent_empty) or (vbatt<100):
        ugfx.input_init()
        welcome()
        ugfx.input_attach(ugfx.BTN_START, start_launcher)
        global splashTimer
        setup_services()
        start_sleep_counter()
        full_clear = False
        if (get_reboot_counter()==0):
            full_clear = True
        draw_home(percent, cstate, "", full_clear, False)
    else:
        draw_batterylow(percent)       
        badge_sleep_forever()
  
#Global variables
splashTimer = machine.Timer(0)
services = []
timer_loop_amount = 10
loopCnt = timer_loop_amount
header_fg = ugfx.BLACK
header_bg = ugfx.WHITE
sleep_duration = 60000
header_hide_while_sleeping = False
header_hide_battery_while_sleeping = False
battery_volt_min = 3800
battery_volt_max = 4300
battery_percent_empty = 1
ntp_timeout = 40
bpp_after_count = 5
header_status_string = ""
splash_timer_interval = 500

splash_main()
 
