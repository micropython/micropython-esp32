# SETUP APPLICATION
# SHOWN ON FIRST BOOT

import ugfx, badge, appglue, dialogs, utime

# Globals

nickname = ""

def load_settings():
    global nickname
    nickname = badge.nvs_get_str("owner", "name", "")
    
def store_settings():
    global nickname
    nickname_new = badge.nvs_set_str("owner", "name", nickname)
    if (nickname_new):
        nickname = nickname_new
        
def check_developer():
    global nickname
    if (nickname==""):
        badge.nvs_set_str('badge', 'setup.state', '2') # Skip the sponsors
        return True
    return False
    
def ask_nickname():
    global nickname
    nickname_new = dialogs.prompt_text("Nickname", nickname)
    if (nickname_new):
        nickname = nickname_new

def action_home(pressed):
    if (pressed):
        appglue.start_app("")
  
def set_setup_state():
    s_old = int(badge.nvs_get_str('badge', 'setup.state', '0'))
    s_new = 2
    if (s_old==0):
        s_new = 1
    badge.nvs_set_str('badge', 'setup.state', str(s_new))
  
def draw_setup_completed():
    ugfx.clear(ugfx.WHITE)
    ugfx.string(0, 0, "Setup", "PermanentMarker22", ugfx.BLACK)
    ugfx.string(0, 25, "Settings stored to flash!", "Roboto_Regular12", ugfx.BLACK)
    ugfx.set_lut(ugfx.LUT_FASTER)
    ugfx.flush()
            
def return_to_home():
    badge.eink_busy_wait()
    appglue.start_app("")

def program_main():  
    ugfx.init()            # We need graphics
    load_settings()        # Load current settings
    ask_nickname()         # Ask the nickname
    if not check_developer():
        store_settings()       # Store the settings
        set_setup_state()      # Do the firstboot magic
        draw_setup_completed() # Show the user that we are done
        utime.sleep(2)         # Sleep 2 seconds
    return_to_home()       # Return to the splash app
    
# Start main application
program_main()
 
