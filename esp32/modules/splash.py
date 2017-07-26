import ugfx, time, ntp, badge, machine, appglue, deepsleep, network, esp, gc, services

# SHA2017 badge home screen
#   v2 Thomas Roos
#   v1 Renze Nicolai

def set_time_ntp():
    draw_msg("Configuring clock...")
    if connectWiFi():
        draw_msg("Setting time over NTP...")
        ntp.set_NTP_time()
        draw_msg("Time set!")
        return True
    else:
        draw_msg('RTC not set! :(')
        return False

def draw_msg(msg):
    global line_number
    try:
        line_number
    except:
        line_number = 0
        ugfx.clear(ugfx.WHITE)
        ugfx.string(0, 0, 'Still Loading Anyway...', "PermanentMarker22", ugfx.BLACK)
        ugfx.set_lut(ugfx.LUT_FASTER)
        draw_msg(msg)
    else:
        ugfx.string(0, 30 + (line_number * 15), msg, "Roboto_Regular12", ugfx.BLACK)
        ugfx.flush()
        line_number += 1

def draw_home(do_BPP):

    vBatt = badge.battery_volt_sense()
    vBatt += vDrop

    ugfx.clear(ugfx.WHITE)

    width = round((vBatt-vMin) / (vMax-vMin) * 38)
    if width < 0:
        width = 0
    elif width > 38:
        width = 38

    ugfx.box(2,2,40,18,ugfx.BLACK)
    ugfx.box(42,7,2,8,ugfx.BLACK)
    ugfx.area(3,3,width,16,ugfx.BLACK)

    if vBatt > 500:
        if badge.battery_charge_status():
            bat_status = 'Charging...'
        else:
            bat_status = str(round(vBatt/1000, 2)) + 'v'
    else:
        bat_status = 'No battery'

    ugfx.string(47, 2, bat_status,'Roboto_Regular18',ugfx.BLACK)


    if do_BPP:
        info = '[ ANY: Wake up ]'
    elif OTA_available:
        info = '[ SELECT: UPDATE | START: LAUNCHER ]'
        ugfx.string(0, 115, 'OTA ready!', 'Roboto_Regular12', ugfx.BLACK)
    else:
        info = '[ START: LAUNCHER ]'

    l = ugfx.get_string_width(info,"Roboto_Regular12")
    ugfx.string(296-l, 115, info, "Roboto_Regular12",ugfx.BLACK)

    ugfx.string(0, 40, nick, "PermanentMarker36", ugfx.BLACK)
    services.draw()

    ugfx.flush(ugfx.LUT_FULL)

    if do_BPP:
        badge.eink_busy_wait()
        # appglue.start_bpp() ## SHOULD BE THIS!!
        deepsleep.start_sleeping()
    else:
        ugfx.input_attach(ugfx.BTN_SELECT, start_ota)

def start_ota(pushed):
    if pushed:
        appglue.start_ota()

def press_nothing(pushed):
    if pushed:
        global loopCount
        loopCount = badge.nvs_get_u8('splash', 'timer.amount', 50)

def press_start(pushed):
    if pushed:
        appglue.start_app("launcher", False)

def press_a(pushed):
    if pushed:
        global loopCount
        global magic
        loopCount = badge.nvs_get_u8('splash', 'timer.amount', 50)
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

def splashTimer_callback(tmr):
    global loopCount
    try:
        loopCount
    except:
        loopCount = badge.nvs_get_u8('splash', 'timer.amount', 50)
        draw_home(False)
    else:
            if loopCount < 1 and badge.usb_volt_sense() < 4500:
                draw_home(True)
            else:
                if not services.loop(loopCount):
                    loopCount -= 1

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

        draw_msg("Connecting to '"+ssid+"'...")

        timeout = badge.nvs_get_u8('splash', 'wifi.timeout', 40)
        while not nw.isconnected():
            time.sleep(0.1)
            timeout = timeout - 1
            if (timeout<1):
                draw_msg("Timeout while connecting!")
                disableWiFi()
                return False
    return True

def download_ota_info():
    import urequests as requests
    draw_msg("Downloading OTA status...")
    result = False
    try:
        data = requests.get("https://badge.sha2017.org/version")
    except:
        draw_msg("Could not download JSON!")
        time.sleep(5)
        return False
    try:
        result = data.json()
    except:
        data.close()
        draw_msg("Could not decode JSON!")
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
                badge.nvs_set_u8('badge','OTA.ready',1)
                return True
            else:
                badge.nvs_set_u8('badge','OTA.ready',0)
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
    setupcompleted = badge.nvs_get_u8('badge', 'setup.state', 0)
    if setupcompleted == 0: # First boot (open setup)
        print("[SPLASH] Setup not completed. Running setup!")
        appglue.start_app("setup")
    elif setupcompleted == 1: # Second boot (after setup)
        print("[SPLASH] Showing sponsors once...")
        badge.nvs_set_u8('badge', 'setup.state', 2) # Only force show sponsors once
        appglue.start_app("sponsors")
    elif setupcompleted == 2:
        badge.nvs_set_u8('badge', 'setup.state', 3)
        check_ota_available()
    else: # Setup completed
        print("[SPLASH] Normal boot.")

nick = badge.nvs_get_str("owner", "name", 'Jan de Boer')
vMin = badge.nvs_get_u16('splash', 'bat.volt.min', 3600) # mV
vMax = badge.nvs_get_u16('splash', 'bat.volt.max', 4200) # mV
if badge.battery_charge_status() == False and badge.usb_volt_sense() > 4500 and badge.battery_volt_sense() > 2500:
    badge.nvs_set_u16('splash', 'bat.volt.drop', 5200 - badge.battery_volt_sense()) # mV
    print('Set vDrop to: ' + str(4200 - badge.battery_volt_sense()))
vDrop = badge.nvs_get_u16('splash', 'bat.volt.drop', 0) - 1000 # mV

inputInit()
magic = 0

checkFirstBoot()

#If clock on time before 2017
doOTA = set_time_ntp() if time.time() < 1482192000 else True

if (machine.reset_cause() != machine.DEEPSLEEP_RESET) and doOTA:
    OTA_available = check_ota_available()
else:
    OTA_available = badge.nvs_get_u8('badge','OTA.ready',0)

ugfx.clear(ugfx.WHITE)
ugfx.flush(ugfx.LUT_FASTER)

foundService = services.setup()

splashTimer = machine.Timer(-1)
splashTimer.init(period=badge.nvs_get_u16('splash', 'timer.period', 100), mode=machine.Timer.PERIODIC, callback=splashTimer_callback)

gc.collect()
