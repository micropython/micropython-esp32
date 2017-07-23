import ugfx, time, badge, machine, uos, appglue, deepsleep, network, esp, gc

# SHA2017 badge home screen
# Renze Nicolai 2017

# Reboot counter (Used for BPP)
def get_reboot_counter():
    return esp.rtcmem_read(1023)

def set_reboot_counter(value):
    esp.rtcmem_write(1023, value)

def increment_reboot_counter():
    val = get_reboot_counter()
    if (val<255):
        val = val + 1
    set_reboot_counter(val)

# BPP
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


def get_time_ntp():
    draw_msg("Configuring clock...", "Welcome!")
    if (connectWiFi()):
        draw_msg("Configuring clock...", "NTP...")
        ntp.set_NTP_time()
        draw_msg("Configuring clock...", "Done!")
        return True
    else:
        return False

# BATTERY
def battery_percent(vbatt):
        global battery_volt_min
        global battery_volt_max
        percent = round(((vbatt-vempty)*100)/(vfull-vempty))
        if (percent<0):
            percent = 0
        elif (percent>100):
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

def draw_home(full_clear, going_to_sleep):
    global update_available
    global update_name
    global update_build

    if (update_available):
        status = "OTA update available"

    draw_helper_clear(full_clear)       # CLEAR SCREEN
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
    elif (status=="BPP"):
        info = "[ ANY: Exit BPP ]"
    else:
        info = "[ START: LAUNCHER ]"

    clock = clockstring()

    if (update_available):
        ugfx.string(0, 25, "Update to version "+str(update_build)+ " '"+update_name+"' available","Roboto_Regular12",ugfx.BLACK)
        info = "[ B: OTA UPDATE ] "+info
        clock = ""

    draw_helper_footer(clock,info)
    draw_helper_nick("Unknown")
    draw_services()
    draw_helper_flush(True)

# START LAUNCHER
def start_launcher(pushed):
    if(pushed):
        print("[SPLASH] Starting launcher...")
        global splashTimer
        splashTimer.deinit()
        increment_reboot_counter()
        appglue.start_app("launcher")

def start_ota(pushed):
    if(pushed):
        print("[SPLASH] Starting OTA...")
        appglue.start_ota()

# NOTHING
def nothing(pressed):
    if (pressed):
        reset_countdown()

# MAGIC
def actually_start_magic():
    print("[SPLASH] Starting magic...")
    esp.rtcmem_write_string("magic")
    deepsleep.reboot()

magic = 0
def start_magic(pushed):
    reset_countdown()
    global magic
    if(pushed):
        magic = magic + 1
        if (magic>10):
            actually_start_magic()
        else:
            print("[SPLASH] Magic in "+str(10-magic)+"...")

# SLEEP
def badge_sleep():
    increment_reboot_counter()
    global sleep_duration
    print("[SPLASH] Going to sleep now...")
    badge.eink_busy_wait() #Always wait for e-ink
    deepsleep.start_sleeping(sleep_duration*1000)

def sleepIfEmpty(vbatt):
    global battery_volt_min
    if (vbatt > 100) and (vbatt < battery_volt_min):
        increment_reboot_counter()
        print("[SPLASH] Going to sleep WITHOUT TIME WAKEUP now...")
        badge.eink_busy_wait() #Always wait for e-ink
        deepsleep.start_sleeping(0) #Sleep until button interrupt occurs
    else:
        return True

# TIMER
def splashTimer_callback(tmr):
    global loopCnt
    global timer_loop_amount
    #print("[TIMER] "+str(loopCnt))
    if loopCnt<1:
        loopCnt = timer_loop_amount
        draw_home(False, True)
    else:
        if not (loop_services(loopCnt)):
            loopCnt = loopCnt - 1

def reset_countdown():
    global loopCnt
    global timer_loop_amount
    loopCnt = timer_loop_amount

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

# WIFI
def disableWiFi():
    nw = network.WLAN(network.STA_IF)
    nw.active(False)

def connectWiFi():
    nw = network.WLAN(network.STA_IF)
    if not nw.isconnected():
        nw.active(True)
        ssid = badge.nvs_get_str('badge', 'wifi.ssid', 'SHA2017-insecure')
        password = badge.nvs_get_str('badge', 'wifi.password')
        if password:
            sta_if.connect(ssid, password)
        else:
            sta_if.connect(ssid)

        draw_msg("Wi-Fi active", "Connecting to '"+ssid+"'...")

        global wifi_timeout
        timeout = wifi_timeout
        while not nw.isconnected():
            time.sleep(0.1)
            timeout = timeout - 1
            if (timeout<1):
                draw_msg("Error", "Timeout while connecting!")
                disableWiFi()
                time.sleep(1)
                return False
    return True

# CHECK OTA VERSION

def download_ota_info():
    import urequests as requests
    draw_msg("Loading...", "Downloading JSON...")
    result = False
    try:
        data = requests.get("https://badge.sha2017.org/version")
    except:
        draw_msg("Error", "Could not download JSON!")
        time.sleep(5)
        return False
    try:
        result = data.json()
    except:
        data.close()
        draw_msg("Error", "Could not decode JSON!")
        time.sleep(5)
        return False
    data.close()
    return result

def check_ota_available():
    global update_available
    global update_name
    global update_build

    if connectWiFi():
        json = download_ota_info()
        if (json):
            import version
            if (json["build"] > version.build):
                update_available = True
                update_name = json["name"]
                update_build = json["build"]
                ugfx.input_attach(ugfx.BTN_B, start_ota)
                return True
    return False

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

# SETTINGS FROM NVS
def load_settings():
    global header_status_string
    header_status_string = ""

    header_inv = badge.nvs_get_u8('splash', 'header.invert', 0)
    if (header_inv>0):
        global header_fg
        global header_bg
        header_fg = ugfx.WHITE
        header_bg = ugfx.BLACK
    header_hws = badge.nvs_get_u8('splash', 'header.hws', 0) #Hide While Sleeping
    if (header_hws>0):
        global header_hide_while_sleeping
        header_hide_while_sleeping = True
    header_hbws = badge.nvs_get_u8('splash', 'header.hbws', 0) #Hide Battery While Sleeping
    if (header_hbws>0):
        global header_hide_battery_while_sleeping
        header_hide_battery_while_sleeping = True
    global sleep_duration
    sleep_duration = badge.nvs_get_u8('splash', 'sleep.duration', 60)
    if (sleep_duration<30):
        print("[SPLASH] Sleep duration set to less than 30 seconds. Forcing 30 seconds.")
        sleep_duration = 30
    #if (sleep_duration>120):
    #    print("[SPLASH] Sleep duration set to more than 120 seconds. Forcing 120 seconds.")
    global battery_volt_min
    battery_volt_min = badge.nvs_get_u16('splash', 'bat.volt.min', 3500) # mV
    # Just bellow brownout, real battery voltage may be higher due to voltage drop over polyfuse
    global battery_volt_max
    battery_volt_max = badge.nvs_get_u16('splash', 'bat.volt.max', 4100) # mV
    # Lower than charge voltage, to display (almost) full when battery has stopped charging
    global battery_percent_empty
    battery_percent_empty = badge.nvs_get_u8('splash', 'bat.perc.empty', 25) # %
    global wifi_timeout
    wifi_timeout = badge.nvs_get_u8('splash', 'ntp.timeout', 40) #amount of tries
    global bpp_after_count
    bpp_after_count = badge.nvs_get_u8('splash', 'bpp.count', 5)
    global splash_timer_interval
    splash_timer_interval = badge.nvs_get_u16('splash', 'timer.interval', 200)
    global timer_loop_amount
    timer_loop_amount = badge.nvs_get_u8('splash', 'timer.amount', 25)
    global loopCnt
    loopCnt = timer_loop_amount

# MAIN
def splash_main():

    load_settings()     # Settings

    ugfx.input_init()   # Inputs
    ugfx.input_attach(ugfx.BTN_START, start_launcher)
    ugfx.input_attach(ugfx.BTN_A, start_magic)
    ugfx.input_attach(ugfx.BTN_B, nothing)
    ugfx.input_attach(ugfx.BTN_SELECT, nothing)
    ugfx.input_attach(ugfx.JOY_UP, nothing)
    ugfx.input_attach(ugfx.JOY_DOWN, nothing)
    ugfx.input_attach(ugfx.JOY_LEFT, nothing)
    ugfx.input_attach(ugfx.JOY_RIGHT, nothing)

    welcome()
    gc.collect()

    doOTA = True
    if (time.time() < 1482192000): #If clock on time before 2017
        doOTA = get_time_ntp()

    if (machine.reset_cause() != machine.DEEPSLEEP_RESET) and doOTA:
        print("[SPLASH] Cold boot, checking if OTA available...")
        check_ota_available()

    gc.collect()

    if (get_reboot_counter()==0):
        draw_home(True, False)
    else:
        draw_home(False, False)

    setup_services()
    start_sleep_counter()


# GLOBALS (With defaults that will probably never be used)
splashTimer = machine.Timer(0)
services = []
timer_loop_amount = 9999
loopCnt = timer_loop_amount
header_fg = ugfx.BLACK
header_bg = ugfx.WHITE
sleep_duration = 60
header_hide_while_sleeping = False
header_hide_battery_while_sleeping = False
battery_volt_min = 3800
battery_volt_max = 4300
battery_percent_empty = 1
wifi_timeout = 40
bpp_after_count = 5
header_status_string = ""
splash_timer_interval = 500

update_available = False
update_name = "????"
update_build = 0

splash_main()
