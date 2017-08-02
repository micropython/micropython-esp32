# File: resourcecheck.py
# Version: 1
# Description: Resources check module
# License: MIT
# Authors: Renze Nicolai <renze@rnplus.nl>

import easywifi, easydraw

def install():
    easydraw.msg("Installing resources...")
    if not easywifi.status():
        if not easywifi.enable():
            return False
    import woezel
    woezel.install("resources")
    appglue.home()

def check():
    needToInstall = True
    try:
        fp = open("/lib/resources/version", "r")
        version = int(fp.read(99))
        print("[RESOURCES] Current version: "+str(version))
        if version>=2:
            needToInstall = False
    except:
        pass
    if needToInstall:
        print("[RESOURCESH] Update required!")
        install()
        return True
    return False
