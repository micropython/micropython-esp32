# File: otacheck.py
# Version: 1
# Description: OTA check module
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import easywifi, easydraw, badge

def download_info():
    import urequests as requests
    easydraw.msg("Checking for updates...")
    result = False
    try:
        data = requests.get("https://badge.sha2017.org/version")
    except:
        easydraw.msg("Error: could not download JSON!")
        time.sleep(5)
        return False
    try:
        result = data.json()
    except:
        data.close()
        easydraw.msg("Error: could not decode JSON!")
        time.sleep(5)
        return False
    data.close()
    return result

def available(update=False):
    if update:
        if not easywifi.status():
            if not easywifi.enable():
                return badge.nvs_get_u8('badge','OTA.ready',0)

        info = download_info()
        if info:
            import version
            if info["build"] > version.build:
                badge.nvs_set_u8('badge','OTA.ready',1)
                return True

        badge.nvs_set_u8('badge','OTA.ready',0)
    return badge.nvs_get_u8('badge','OTA.ready',0)
