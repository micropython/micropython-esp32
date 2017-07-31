# File: setup.py
# Version: 3
# Description: Setup for SHA2017 badge
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>
#          Thomas Roos   <?>

import ugfx, badge, appglue, dialogs, easydraw, time

def asked_nickname(value):
    if value:
        badge.nvs_set_str("owner", "name", value)

        # Do the firstboot magic
        newState = 1 if badge.nvs_get_u8('badge', 'setup.state', 0) == 0 else 3
        badge.nvs_set_u8('badge', 'setup.state', newState)

        # Show the user that we are done
        easydraw.msg("Hi "+value+"!", 'Your nick has been stored to flash!')
        time.sleep(0.5)
    else:
        badge.nvs_set_u8('badge', 'setup.state', 2) # Skip the sponsors
        badge.nvs_set_u8('sponsors', 'shown', 1)

    badge.eink_busy_wait()
    appglue.home()

ugfx.init()
nickname = badge.nvs_get_str("owner", "name", "")

dialogs.prompt_text("Nickname", nickname, cb=asked_nickname)
