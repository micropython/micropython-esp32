import ugfx, badge, sys, gc
import uos as os
import uerrno as errno
import ujson as json
import network, wifi
import machine, esp, time
import urequests as requests
import appglue

wifi.init()

ugfx.clear(ugfx.BLACK)
ugfx.string(20,25,"Connecting to:","Roboto_BlackItalic24",ugfx.WHITE)
ugfx.string(140,75, "WiFi","PermanentMarker22",ugfx.WHITE)
ugfx.flush()

timeout = 250
while not wifi.sta_if.isconnected():
	time.sleep(0.1)
	timeout = timeout - 1
	if (timeout<1):
		ugfx.clear(ugfx.BLACK)
		ugfx.string(5,5,"Failure.","Roboto_BlackItalic24",ugfx.WHITE)
		ugfx.flush()
		time.sleep(2)
		appglue.start_app("")
	pass

ugfx.clear(ugfx.WHITE)

def show_description(active):
	if active:
		text.text(packages[options.selected_index()]["description"])
		ugfx.flush()

def empty_options():
	global options
	options.destroy()
	options = ugfx.List(0,0,int(ugfx.width()/2),ugfx.height())

ugfx.input_init()

window = ugfx.Container(0, 0, ugfx.width(), ugfx.height())

options = ugfx.List(0,0,int(ugfx.width()/2),ugfx.height())

text = ugfx.Textbox(int(ugfx.width()/2),0, int(ugfx.width()/2), ugfx.height())
text.text("Downloading category list...")

ugfx.flush(ugfx.LUT_FULL)
badge.eink_busy_wait()
ugfx.set_lut(ugfx.LUT_FASTER)

packages = {} # global variable

gc.collect()

f = requests.get("https://badge.sha2017.org/eggs/categories/json")
try:
	categories = f.json()
finally:
	f.close()

gc.collect()

def show_category(active):
    if active:
		ugfx.string_box(148,0,148,26, "Hatchery", "Roboto_BlackItalic24", ugfx.BLACK, ugfx.justifyCenter)
		text.text("Install or update eggs from the hatchery here.\nSelect a category to start, or press B to return to the launcher.\n\nbadge.sha2017.org")
		ugfx.flush()

def list_categories():
	global options
	global text

	empty_options()
	text.destroy()
	text = ugfx.Textbox(int(ugfx.width()/2),26,int(ugfx.width()/2),ugfx.height()-26)

	ugfx.input_attach(ugfx.JOY_UP, show_category)
	ugfx.input_attach(ugfx.JOY_DOWN, show_category)
	ugfx.input_attach(ugfx.BTN_A, select_category)
	ugfx.input_attach(ugfx.BTN_B, lambda pushed: appglue.start_app("launcher") if pushed else 0)
	ugfx.input_attach(ugfx.BTN_START, lambda pushed: appglue.start_app("") if pushed else 0)

	for category in categories:
		options.add_item("%s (%d) >" % (category["name"], category["eggs"]))

	show_category(True)

def select_category(active):
	if active:
		global categories
		index = options.selected_index()
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

	empty_options()
	text.destroy()
	text = ugfx.Textbox(int(ugfx.width()/2),0, int(ugfx.width()/2), ugfx.height())
	text.text("Downloading list of eggs...")
	ugfx.flush(ugfx.LUT_FULL)
	badge.eink_busy_wait()

	gc.collect()
	f = requests.get("https://badge.sha2017.org/eggs/category/%s/json" % slug)
	try:
		packages = f.json()
	finally:
		f.close()

	gc.collect()

	for package in packages:
		options.add_item("%s rev. %s" % (package["name"], package["revision"]))

	ugfx.input_attach(ugfx.JOY_UP, show_description)
	ugfx.input_attach(ugfx.JOY_DOWN, show_description)
	ugfx.input_attach(ugfx.BTN_A, install_app)
	ugfx.input_attach(ugfx.BTN_B, lambda pushed: appglue.start_app("installer") if pushed else 0)
	ugfx.input_attach(ugfx.BTN_START, lambda pushed: appglue.start_app("installer") if pushed else 0)

	show_description(True)
	badge.eink_busy_wait()
	ugfx.set_lut(ugfx.LUT_FASTER)

def install_app(active):
	if active:
		global options
		global text
		global packages

		index = options.selected_index()

		options.destroy()
		text.destroy()

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
		woezel.install(packages[index]["slug"])

		ugfx.clear(ugfx.WHITE)
		ugfx.string(40,25,"Installed:","Roboto_BlackItalic24",ugfx.BLACK)
		ugfx.string(100,55, packages[index]["name"],"PermanentMarker22",ugfx.BLACK)
		text = ugfx.Textbox(0,100, ugfx.width(), ugfx.height()-100)
		text.text("Press A to start %s, or B to return to the installer" % packages[index]["name"])
		
		ugfx.input_attach(ugfx.BTN_A, lambda pushed: appglue.start_app(packages[index]["slug"]) if pushed else 0)
		ugfx.input_attach(ugfx.BTN_B, lambda pushed: appglue.start_app("installer"))
		ugfx.input_attach(ugfx.BTN_START, lambda pushed: appglue.start_app(""))

		ugfx.flush()


list_categories()
