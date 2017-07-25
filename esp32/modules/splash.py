import ugfx, time, ntp, badge, machine, appglue, deepsleep, network, esp, gc, services

# SHA2017 badge home screen
#   Renze Nicolai
#   Thomas Roos
# TIME

def set_time_ntp():
    draw_msg("Configuring clock...", "Connecting to WiFi...")
    if connectWiFi():
        draw_msg("Configuring clock...", "Setting time over NTP...")
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
    ugfx.set_lut(ugfx.LUT_FASTEST)
    ugfx.flush()


def draw_home(do_BPP):

    vBatt = badge.battery_volt_sense()
    vBatt += vDrop

    width = (vBatt-vMin) / (vMax-vMin) * 38
    if width < 0:
        width = 0
    elif width < 38:
        width = 38


    ugfx.box(2,2,40,18,ugfx.BLACK)
    ugfx.box(42,7,2,8,ugfx.BLACK)
    ugfx.area(3,3,width,16,ugfx.BLACK)


    if badge.battery_charge_status():
        bat_status = 'Charging...'
    elif vBatt > 100:
        bat_status = str(round(vBatt/1000, 2)) + 'v'
    else:
        bat_status = 'No battery'

    ugfx.string(47, 2, bat_status,'Roboto_Regular18',ugfx.BLACK)



    if do_BPP:
        info = "[ ANY: Wake up ]"
    elif OTA_available:
        info = "[ B: OTA ] [ START: LAUNCHER ]"
        ugfx.string(0, 108, 'OTA ready!', "Roboto_Regular18",ugfx.BLACK)
    else:
        info = "[ START: LAUNCHER ]"

    l = ugfx.get_string_width(info,"Roboto_Regular12")
    ugfx.string(296-l, 115, info, "Roboto_Regular12",ugfx.BLACK)

    ugfx.string(0, 40, nick, "PermanentMarker36", ugfx.BLACK)
    services.draw()

    ugfx.flush()

# START LAUNCHER
def press_start(pushed):
    if pushed:
        appglue.start_app("launcher", False)

def start_ota(pushed):
    if pushed:
        appglue.start_ota()

# NOTHING
def press_nothing(pushed):
    if pushed:
        loopCnt = badge.nvs_get_u8('splash', 'timer.amount', 25)

def press_a(pushed):
    if pushed:
        loopCnt = badge.nvs_get_u8('splash', 'timer.amount', 25)
        magic += 1
        if magic > 9:
            appglue.start_app('magic', False)
        else:
            print("[SPLASH] Magic in "+str(10-magic)+"...")


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
    try:
        loopCount
    except NameError:
        loopCount = badge.nvs_get_u8('splash', 'timer.amount', 25)
        draw_home(False)
    else:
            if loopCnt<1:
                draw_home(True)
            else:
                if not services.loop(loopCnt):
                    loopCnt -= 1

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
        nw.connect(ssid, password) if password else nw.connect(ssid)

        draw_msg("Wi-Fi actived", "Connecting to '"+ssid+"'...")

        timeout = badge.nvs_get_u8('splash', 'wifi.timeout', 40)
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
    if connectWiFi():
        json = download_ota_info()
        if (json):
            import version
            if (json["build"] > version.build):
                ugfx.input_attach(ugfx.BTN_B, start_ota)
                return True
    return False

def inputInit():
    ugfx.input_init()
    ugfx.input_attach(ugfx.BTN_START, press_start)
    ugfx.input_attach(ugfx.BTN_A, press_a)
    ugfx.input_attach(ugfx.BTN_B, press_nothing)
    ugfx.input_attach(ugfx.BTN_SELECT, press_nothing)
    ugfx.input_attach(ugfx.JOY_UP, press_nothing)
    ugfx.input_attach(ugfx.JOY_DOWN, press_nothing)
    ugfx.input_attach(ugfx.JOY_LEFT, press_nothing)
    ugfx.input_attach(ugfx.JOY_RIGHT, press_nothing)

def checkFirstBoot():
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

header_inv = badge.nvs_get_u8('splash', 'header.invert', 0)
nick = badge.nvs_get_str("owner", "name", 'John Doe')
vMin = badge.nvs_get_u16('splash', 'bat.volt.min', 3600) # mV
vMax = badge.nvs_get_u16('splash', 'bat.volt.max', 4200) # mV
vDrop = badge.nvs_get_u16('splash', 'bat.volt.drop', 100) # mV

inputInit()
magic = 0

checkFirstBoot()

#If clock on time before 2017
doOTA = set_time_ntp() if time.time() < 1482192000 else True

if (machine.reset_cause() != machine.DEEPSLEEP_RESET) and doOTA:
    OTA_available = check_ota_available()
else:
    OTA_available = False

disableWiFi()

foundService = services.setup()

splashTimer = machine.Timer(-1)
splashTimer.init(period=badge.nvs_get_u16('splash', 'timer.period', 100), mode=machine.Timer.PERIODIC, callback=splashTimer_callback)

gc.collect()
