# File: sponsorscheck.py
# Version: 1
# Description: Sponsors check module
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import easywifi, easydraw, appglue, badge

def install():
    if not easywifi.status():
        if not easywifi.enable():
            return False
    print("[SPONSORS] Installing...")
    easydraw.msg("Installing sponsors...")
    import woezel
    woezel.install("sponsors")
    easydraw.msg("OK")

def show(force=False):
    if (not badge.nvs_get_u8('sponsors', 'shown', 0)) or force:
        needToInstall = True
        version = 0
        try:
            fp = open("/lib/sponsors/version", "r")
            version = int(fp.read(99))
            print("[SPONSORS] Current version: "+str(version))
        except:
            print("[SPONSORS] Not installed!")
        if version>=14:
            needToInstall = False
        if needToInstall:
            install()
        try:
            fp = open("/lib/sponsors/version", "r")
            version = int(fp.read(99))
            # Now we know for sure that a version of the sponsors app has been installed
            badge.nvs_set_u8('sponsors', 'shown', 1)
            appglue.start_app("sponsors")
        except:
            pass
