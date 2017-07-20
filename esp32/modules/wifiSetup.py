import dialogs, ugfx, network, badge, deepsleep

def clearGhosting():
	ugfx.clear(ugfx.WHITE)
	ugfx.flush()
	badge.eink_busy_wait()
	ugfx.clear(ugfx.BLACK)
	ugfx.flush()
	badge.eink_busy_wait()

ugfx.init()
ugfx.input_init()

clearGhosting()
ugfx.clear(ugfx.WHITE)
ugfx.string(100,50,'Scanning...','Roboto_Regular18',ugfx.BLACK)
ugfx.flush()

sta_if = network.WLAN(network.STA_IF); sta_if.active(True)
scanResults = sta_if.scan()

ssidList = []
for AP in scanResults:
    ssidList.append(AP[0])
ssidSet = set(ssidList)

clearGhosting()
ugfx.clear(ugfx.WHITE)
ugfx.string(25,20,'Found','Roboto_Regular18',ugfx.BLACK)
ugfx.string(40,40,str(len(ssidSet)),'Roboto_Regular18',ugfx.BLACK)
ugfx.string(10,60,'Networks','Roboto_Regular18',ugfx.BLACK)
options = ugfx.List(ugfx.width()-int(ugfx.width()/1.5),0,int(ugfx.width()/1.5),ugfx.height())

for ssid in ssidSet:
    options.add_item(ssid)

def connectClick(pushed):
    if pushed:
        selected = options.selected_text().encode()
        print('selected')
        options.destroy()

        ssidType = scanResults[ssidList.index(selected)][4]
        print(ssidType)
        print(ssidList.index(selected))
        ugfx.clear(ugfx.WHITE)
        ugfx.string(100,50,selected,'Roboto_Regular18',ugfx.BLACK)
        ugfx.flush()
        if ssidType == 5:
            clearGhosting()
            ugfx.clear(ugfx.WHITE)
            ugfx.string(20,50,'WPA Enterprise unsupported...','Roboto_Regular18',ugfx.BLACK)
            ugfx.set_lut(ugfx.LUT_FULL)
            ugfx.flush()
            badge.eink_busy_wait()
            deepsleep.start_sleeping(1)

        badge.nvs_set_str("badge", "wifi.ssid", selected)

        if ssidType == 0:
			badge.nvs_set_str("badge", "wifi.password", '')
			deepsleep.start_sleeping(1)
        else:
        	clearGhosting()
        	dialogs.prompt_text("WiFi password", cb = passInputDone)


def passInputDone(passIn):
    badge.nvs_set_str("badge", "wifi.password", passIn)
    ugfx.clear(ugfx.WHITE)
    ugfx.string(100,50,'Restarting!','Roboto_Regular18',ugfx.BLACK)
    ugfx.flush()
    badge.eink_busy_wait()
    deepsleep.start_sleeping(1)

ugfx.input_attach(ugfx.BTN_A, connectClick)
ugfx.input_attach(ugfx.JOY_UP, lambda pushed: ugfx.flush() if pushed else 0)
ugfx.input_attach(ugfx.JOY_DOWN, lambda pushed: ugfx.flush() if pushed else 0)

ugfx.set_lut(ugfx.LUT_FULL)
ugfx.flush()
ugfx.set_lut(ugfx.LUT_FASTER)
