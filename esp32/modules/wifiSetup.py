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

clearGhosting()
ugfx.clear(ugfx.WHITE)
options = ugfx.List(ugfx.width()-int(ugfx.width()/1.5),0,int(ugfx.width()/1.5),ugfx.height())

for AP in scanResults:
    options.add_item(AP[0])

def connectClick(pushed):
    if pushed:
        selected = options.selected_index()
        options.destroy()

        ugfx.clear(ugfx.WHITE)
        ugfx.string(100,50,scanResults[selected][0],'Roboto_Regular18',ugfx.BLACK)
        ugfx.flush()
        if scanResults[selected][4] == 5:
            clearGhosting()
            ugfx.clear(ugfx.WHITE)
            ugfx.string(20,50,'WPA Enterprise unsupported...','Roboto_Regular18',ugfx.BLACK)
            ugfx.set_lut(ugfx.LUT_FULL)
            ugfx.flush()
            badge.eink_busy_wait()
            deepsleep.start_sleeping(1)
        
        badge.nvs_set_str("badge", "wifi.ssid", scanResults[selected][0])
        
        if scanResults[selected][4] == 0:
			badge.nvs_set_str("badge", "wifi.password", '')
			deepsleep.start_sleeping(1)
        else:
        	clearGhosting()
        	dialogs.prompt_text("WiFi password", cb = passInputDone)
        

def passInputDone(passIn):
    ugfx.clear(ugfx.WHITE)
    ugfx.string(100,50,'Restarting!','Roboto_Regular18',ugfx.BLACK)
    ugfx.string(100,10,passIn,'Roboto_Regular18',ugfx.BLACK)
    ugfx.flush()
    badge.eink_busy_wait()
    deepsleep.start_sleeping(1)

ugfx.input_attach(ugfx.BTN_A, connectClick)
ugfx.input_attach(ugfx.JOY_UP, lambda pushed: ugfx.flush() if pushed else 0)
ugfx.input_attach(ugfx.JOY_DOWN, lambda pushed: ugfx.flush() if pushed else 0)

ugfx.set_lut(ugfx.LUT_FULL)
ugfx.flush()
ugfx.set_lut(ugfx.LUT_FASTER)
