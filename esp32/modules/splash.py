import ugfx
import time
import badge
from machine import Timer

def battery_percent(vempty, vfull, vbatt):
    percent = round(((vbatt-vempty)*100)/(vfull-vempty))
    if (percent<0):
        percent = 0
    if (percent>100):
        percent = 100
    return percent

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
    ugfx.string(0,  0, str(percent)+"%  - BATTERY LOW, PLEASE CHARGE!","Roboto_Regular12",ugfx.BLACK)
    #ugfx.string(0, 15, "BATTERY LOW! PLEASE CHARGE THE BATTERY!","Roboto_Regular12",ugfx.BLACK)
    nick = badge.nvs_get_str("owner", "name", "Hacker1337")
    ugfx.string(0, 40, nick, "PermanentMarker36", ugfx.BLACK)
    ugfx.set_lut(ugfx.LUT_FASTER)
    ugfx.flush()
    
def start_launcher(pushed):
    global splashTimer
    if(pushed):
        splashTimer.deinit()
        
        #By importing it...
        #import launcher
        
        #Or by sleeping...
        ugfx.clear(ugfx.WHITE)
        ugfx.string(0,  0, "Starting launcher...","Roboto_Regular12",ugfx.BLACK)
        ugfx.flush()
        import time
        time.sleep(0.5)
        import esp
        esp.rtcmem_write_string("launcher")
        esp.start_sleeping(1)
        
def gotosleep():
    import time
    time.sleep(1)
    import deepsleep
    deepsleep.start_sleeping(120000)
        
def splashTimer_callback(tmr):
    # Battery status
    cstate = badge.battery_charge_status()
    vbatt = badge.battery_volt_sense()
    percent = battery_percent(3800, 4300, vbatt)
    
    # Depending on battery status...
    if (cstate) or (percent>95): # Charging or full...
        draw_home(percent, cstate, "Press START!") 
    else: # If on battery power...
        if (percent<10): # ... and battery is empty
           draw_batterylow(percent) 
           ugfx.flush()
        else: # ... and battery is okay
           draw_home(percent, cstate, "Badge is sleeping...")
           
        gotosleep()
            
def splash_main():   
    # Battery status
    cstate = badge.battery_charge_status()
    vbatt = badge.battery_volt_sense()
    percent = battery_percent(3800, 4300, vbatt)
        
    # Init graphics and show homescreen
    ugfx.init()
    
    # Depending on battery status...
    if (cstate) or (percent>9):
        draw_home(percent, cstate, "Press START!")
                
        # Accept input
        ugfx.input_init()
        ugfx.input_attach(ugfx.BTN_START, start_launcher)
        
        # Start timer
        global splashTimer
        splashTimer.init(period=5000, mode=Timer.PERIODIC, callback=splashTimer_callback)
        
    else:
        draw_batterylow(percent)       
        gotosleep()
  
# Globals
splashTimer = Timer(-1)

# Start main application
splash_main()
 
