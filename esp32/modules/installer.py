import ugfx, badge, network, gc, time, urequests, appglue

# SHA2017 Badge installer
# V2 Thomas Roos
# V1 Niek Blankers

def draw_msg(msg):
    global line_number
    try:
        line_number
    except:
        line_number = 0
        ugfx.clear(ugfx.WHITE)
        ugfx.string(0, 0, 'Still Loading Anyway...', "PermanentMarker22", ugfx.BLACK)
        ugfx.set_lut(ugfx.LUT_FASTER)
        draw_msg(msg)
    else:
        ugfx.string(0, 30 + (line_number * 15), msg, "Roboto_Regular12", ugfx.BLACK)
        ugfx.flush()
        line_number += 1

def connectWiFi():
    nw = network.WLAN(network.STA_IF)
    if not nw.isconnected():
        nw.active(True)
        ssid = badge.nvs_get_str('badge', 'wifi.ssid', 'SHA2017-insecure')
        password = badge.nvs_get_str('badge', 'wifi.password')
        nw.connect(ssid, password) if password else nw.connect(ssid)

        draw_msg("Connecting to '"+ssid+"'...")

        timeout = badge.nvs_get_u8('splash', 'wifi.timeout', 40)
        while not nw.isconnected():
            time.sleep(0.1)
            timeout = timeout - 1
            if (timeout<1):
                draw_msg("Timeout while connecting!")
                nw.active(True)
                return False
    return True

def show_description(active):
	if active:
		global text
		text.text(packages[options.selected_index()]["description"])
		ugfx.flush()

def select_category(active):
	if active:
		global categories
		global options
		index = options.selected_index()
		if categories[index]["eggs"] > 0:
			category = categories[index]["slug"]
			list_apps(category)

def list_apps(slug):
	global options
	global text
	global packages

	ugfx.input_attach(ugfx.JOY_UP, 0)
	ugfx.input_attach(ugfx.JOY_DOWN, 0)
	ugfx.input_attach(ugfx.BTN_A, 0)
	ugfx.input_attach(ugfx.BTN_B, 0)
	ugfx.input_attach(ugfx.BTN_START, 0)

	while options.count() > 0:
		options.remove_item(0)
	text.text("Downloading list of eggs...")
	ugfx.flush(ugfx.LUT_FULL)

	try:
		f = urequests.get("https://badge.sha2017.org/eggs/category/%s/json" % slug)
		packages = f.json()
	finally:
		f.close()

	for package in packages:
		options.add_item("%s rev. %s" % (package["name"], package["revision"]))
	options.selected_index(0)

	ugfx.input_attach(ugfx.JOY_UP, show_description)
	ugfx.input_attach(ugfx.JOY_DOWN, show_description)
	ugfx.input_attach(ugfx.BTN_A, install_app)
	ugfx.input_attach(ugfx.BTN_B, lambda pushed: list_categories() if pushed else False)
	ugfx.input_attach(ugfx.BTN_START, lambda pushed: appglue.start_app('') if pushed else False)

	show_description(True)
	gc.collect()

def start_categories(pushed):
	if pushed:
		list_categories()

def start_app(pushed):
	if pushed:
		global selected_app
		appglue.start_app(selected_app)

def install_app(active):
	if active:
		global options
		global text
		global packages
		global selected_app

		index = options.selected_index()

		ugfx.input_attach(ugfx.JOY_UP, 0)
		ugfx.input_attach(ugfx.JOY_DOWN, 0)
		ugfx.input_attach(ugfx.BTN_A, 0)
		ugfx.input_attach(ugfx.BTN_B, 0)
		ugfx.input_attach(ugfx.BTN_START, 0)

		ugfx.clear(ugfx.BLACK)
		ugfx.string(40,25,"Installing:","Roboto_BlackItalic24",ugfx.WHITE)
		ugfx.string(100,55, packages[index]["name"],"PermanentMarker22",ugfx.WHITE)
		ugfx.flush()

		import woezel
		selected_app =  packages[index]["slug"]
		woezel.install(selected_app)

		ugfx.clear(ugfx.WHITE)
		ugfx.string(40,25,"Installed:","Roboto_BlackItalic24",ugfx.BLACK)
		ugfx.string(100,55, packages[index]["name"],"PermanentMarker22",ugfx.BLACK)
		ugfx.string(0, 115, "[ A: START | B: BACK ]", "Roboto_Regular12", ugfx.BLACK)

		ugfx.input_attach(ugfx.BTN_A, start_app)
		ugfx.input_attach(ugfx.BTN_B, start_categories)
		ugfx.input_attach(ugfx.BTN_START, lambda pushed: appglue.start_app("") if pushed else False)

		ugfx.flush()
		gc.collect()

def list_categories():
	global options
	global text
	global categories

	try:
		categories
	except:
		ugfx.input_init()
		draw_msg('Getting categories')
		try:
			f = urequests.get("https://badge.sha2017.org/eggs/categories/json")
			categories = f.json()
		except:
			draw_msg('Failed!')
			draw_msg('Returning to launcher :(')
			appglue.start_app('launcher', False)

			f.close()
		draw_msg('Done!')
		options = ugfx.List(0,0,int(ugfx.width()/2),ugfx.height())
		text = ugfx.Textbox(int(ugfx.width()/2),0, int(ugfx.width()/2), ugfx.height())



	ugfx.input_attach(ugfx.JOY_UP, lambda pushed: ugfx.flush() if pushed else False)
	ugfx.input_attach(ugfx.JOY_DOWN, lambda pushed: ugfx.flush() if pushed else False)
	ugfx.input_attach(ugfx.BTN_A, select_category)
	ugfx.input_attach(ugfx.BTN_B, lambda pushed: appglue.start_app("launcher", False) if pushed else False)
	ugfx.input_attach(ugfx.BTN_START, lambda pushed: appglue.start_app("") if pushed else False)

	ugfx.clear(ugfx.WHITE)
	ugfx.flush()

	while options.count() > 0:
		options.remove_item(0)
	for category in categories:
		options.add_item("%s (%d) >" % (category["name"], category["eggs"]))
	options.selected_index(0)

	text.text("Install or update eggs from the hatchery here\n\n\n\n")
	ugfx.string_box(148,0,148,26, "Hatchery", "Roboto_BlackItalic24", ugfx.BLACK, ugfx.justifyCenter)
	ugfx.line(148, 78, 296, 78, ugfx.BLACK)
	ugfx.string_box(148,78,148,18, " A: Open category", "Roboto_Regular12", ugfx.BLACK, ugfx.justifyLeft)
	ugfx.string_box(148,92,148,18, " B: Return to home", "Roboto_Regular12", ugfx.BLACK, ugfx.justifyLeft)
	ugfx.line(148, 110, 296, 110, ugfx.BLACK)
	ugfx.string_box(148,110,148,18, " badge.sha2017.org", "Roboto_Regular12", ugfx.BLACK, ugfx.justifyLeft)
	ugfx.flush(ugfx.LUT_FULL)
	gc.collect()


if not connectWiFi():
	draw_msg('Returning to launcher :(')
	appglue.start_app('launcher', False)
else:
	list_categories()
