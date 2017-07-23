# SETUP APPLICATION
# SHOWN ON FIRST BOOT

import ugfx, badge, appglue, dialogs, utime

def load_settings():
    return badge.nvs_get_str("owner", "name", "")

def store_settings(nickname):
    badge.nvs_set_str("owner", "name", nickname)

def is_developer(nickname):
    if (nickname==""):
        badge.nvs_set_u8('badge', 'setup.state', 2) # Skip the sponsors
        return True
    return False

def action_home(pressed):
    if (pressed):
        appglue.start_app("")

def set_setup_state():
    s_old = badge.nvs_get_u8('badge', 'setup.state', 0)
    s_new = 2
    if (s_old==0):
        s_new = 1
    badge.nvs_set_u8('badge', 'setup.state', s_new)

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
    ugfx.init()
    nickname = load_settings()

    def asked_nickname(value):
        nickname = value if value else nickname
        if not is_developer(nickname):
            store_settings(nickname)
            # Do the firstboot magic
            set_setup_state()
            # Show the user that we are done
            draw_setup_completed()
            utime.sleep(2)
        return_to_home()
    
    dialogs.prompt_text("Nickname", nickname, cb=asked_nickname)

# Start main application
program_main()

