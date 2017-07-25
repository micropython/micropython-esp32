# SETUP APPLICATION
# v2 by Thomas Roos
# v1 by Renze Nicolai

import ugfx, badge, appglue, dialogs

def asked_nickname(value):
    if value:
        badge.nvs_set_str("owner", "name", value)

        # Do the firstboot magic
        newState = 1 if badge.nvs_get_u8('badge', 'setup.state', 0) == 0 else 3
        badge.nvs_set_u8('badge', 'setup.state', newState)

        # Show the user that we are done
        ugfx.clear(ugfx.WHITE)
        ugfx.string(0, 0, "Hi, " + value, "PermanentMarker22", ugfx.BLACK)
        ugfx.string(0, 25, "Your nick is stored to flash!", "Roboto_Regular12", ugfx.BLACK)
        ugfx.flush(ugfx.LUT_FASTER)
    else:
        badge.nvs_set_u8('badge', 'setup.state', 2) # Skip the sponsors

    badge.eink_busy_wait()
    appglue.start_app("")

ugfx.init()
nickname = badge.nvs_get_str("owner", "name", "")
dialogs.prompt_text("Nickname", nickname, cb=asked_nickname)
