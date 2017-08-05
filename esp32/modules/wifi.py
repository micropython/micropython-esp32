import network, badge, gc

sta_if = False

def init():
    global sta_if
    sta_if = network.WLAN(network.STA_IF)
    sta_if.active(True)
    ssid = badge.nvs_get_str('badge', 'wifi.ssid', 'SHA2017-insecure')
    password = badge.nvs_get_str('badge', 'wifi.password')
    if password:
        sta_if.connect(ssid, password)
    else:
        sta_if.connect(ssid)

gc.collect()
