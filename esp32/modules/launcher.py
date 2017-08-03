import ugfx, badge, sys, uos as os, appglue, version, easydraw, virtualtimers, tasks.powermanagement as pm, dialogs, time, ujson, sys

# Application list

apps = []

def add_app(app,title,category):
    global apps
    info = {"file":app,"title":title,"category":category}
    apps.append(info)

def populate_apps():
    global apps
    apps = []
    try:
        userApps = os.listdir('lib')
    except OSError:
        userApps = []
    for app in userApps:
        [title,category] = read_info(app)
            
        if app=="resources":
            category = "hidden"

        add_app(app,title,category)
    add_app("installer","Installer","system")
    add_app("setup","Set nickname","system")
    add_app("update","Update apps","system")
    add_app("ota_update","Update firmware","system")
  
# List as shown on screen
currentListTitles = []
currentListTargets = []

def populate_category(category="",system=True):
    global apps
    global currentListTitles
    global currentListTargets
    currentListTitles = []
    currentListTargets = []
    for app in apps:
        if (category=="" or category==app["category"] or (system and app["category"]=="system")) and (not app["category"]=="hidden"):
            currentListTitles.append(app["title"])
            currentListTargets.append(app)
            
def populate_options():
    global options
    options = ugfx.List(0,0,int(ugfx.width()/2),ugfx.height())
    global currentListTitles
    for title in currentListTitles:
        options.add_item(title)

# Read app info
def read_info(app):
    try:
        install_path = get_install_path()
        info_file = "%s/%s/app.json" % (install_path, app)
        print("Reading "+info_file+"...")
        fd = open(info_file)
        information = ujson.loads(fd.read())
        title = information["title"]
        category = information["category"]
        return [title,category]
    except BaseException as e:
        print("[ERROR] Can not read info for app "+app)
        sys.print_exception(e)
        return [app,""]
     
# Uninstaller

def uninstall():
    global options
    selected = options.selected_index()
    options.destroy()
    
    global currentListTitles
    global currentListTargets
        
    if currentListTargets[selected]["category"] == "system":
        #dialogs.notice("System apps can not be removed!","Can not uninstall '"+currentListTitles[selected]+"'")
        easydraw.msg("System apps can not be removed!","Error",True)
        time.sleep(2)
        start()
        return
    
    def perform_uninstall(ok):
        global install_path
        if ok:
            easydraw.msg("Removing "+currentListTitles[selected]+"...", "Still Uninstalling Anyway...",True)
            install_path = get_install_path()
            for rm_file in os.listdir("%s/%s" % (install_path, currentListTargets[selected]["file"])):
                easydraw.msg("Deleting '"+rm_file+"'...")
                os.remove("%s/%s/%s" % (install_path, currentListTargets[selected]["file"], rm_file))
            easydraw.msg("Deleting folder...")
            os.rmdir("%s/%s" % (install_path, currentListTargets[selected]["file"]))
            easydraw.msg("Uninstall completed!")
        start()

    uninstall = dialogs.prompt_boolean('Are you sure you want to remove %s?' % currentListTitles[selected], cb=perform_uninstall)

# Run app
        
def run():
    global options
    selected = options.selected_index()
    options.destroy()
    global currentListTargets
    appglue.start_app(currentListTargets[selected]["file"])

# Path

def expandhome(s):
    if "~/" in s:
        h = os.getenv("HOME")
        s = s.replace("~/", h + "/")
    return s

def get_install_path():
    global install_path
    if install_path is None:
        # sys.path[0] is current module's path
        install_path = sys.path[1]
    install_path = expandhome(install_path)
    return install_path


# Actions        
def input_a(pressed):
    pm.feed()
    if pressed:
        run()
    
def input_b(pressed):
    pm.feed()
    if pressed:
        appglue.home()

def input_select(pressed):
    pm.feed()
    if pressed:
        uninstall()
    
def input_other(pressed):
    pm.feed()
    if pressed:
        ugfx.flush()

# Power management
def pm_cb(dummy):
    appglue.home()

def init_power_management():
    virtualtimers.activate(1000) # Start scheduler with 1 second ticks
    pm.set_timeout(5*60*1000) # Set timeout to 5 minutes
    pm.callback(pm_cb) # Go to splash instead of sleep
    pm.feed() # Feed the power management task, starts the countdown...

# Main application
def start():
    ugfx.input_init()
    ugfx.set_lut(ugfx.LUT_FASTER)
    ugfx.clear(ugfx.BLACK)
    ugfx.flush()
    ugfx.clear(ugfx.WHITE)
    ugfx.flush()

    ugfx.string_box(148,0,148,26, "STILL", "Roboto_BlackItalic24", ugfx.BLACK, ugfx.justifyCenter)
    ugfx.string_box(148,23,148,23, "Hacking", "PermanentMarker22", ugfx.BLACK, ugfx.justifyCenter)
    ugfx.string_box(148,48,148,26, "Anyway", "Roboto_BlackItalic24", ugfx.BLACK, ugfx.justifyCenter)

    #the line under the text
    str_len = ugfx.get_string_width("Hacking","PermanentMarker22")
    line_begin = 148 + int((148-str_len)/2)
    line_end = str_len+line_begin
    ugfx.line(line_begin, 46, line_end, 46, ugfx.BLACK)

    #the cursor past the text
    cursor_pos = line_end+5
    ugfx.line(cursor_pos, 22, cursor_pos, 44, ugfx.BLACK)

    # Instructions
    ugfx.line(148, 78, 296, 78, ugfx.BLACK)
    ugfx.string_box(148,78,148,18, " A: Run", "Roboto_Regular12", ugfx.BLACK, ugfx.justifyLeft)
    ugfx.string_box(148,78,148,18, " B: Return to home", "Roboto_Regular12", ugfx.BLACK, ugfx.justifyRight)
    ugfx.string_box(148,92,148,18, " SELECT: Uninstall", "Roboto_Regular12", ugfx.BLACK, ugfx.justifyLeft)
    ugfx.line(148, 110, 296, 110, ugfx.BLACK)
    ugfx.string_box(148,110,148,18, " " + version.name, "Roboto_Regular12", ugfx.BLACK, ugfx.justifyLeft)

    global options
    global install_path
    options = None
    install_path = None

    ugfx.input_attach(ugfx.BTN_A, input_a)
    ugfx.input_attach(ugfx.BTN_B, input_b)
    ugfx.input_attach(ugfx.BTN_SELECT, input_select)
    ugfx.input_attach(ugfx.JOY_UP, input_other)
    ugfx.input_attach(ugfx.JOY_DOWN, input_other)
    ugfx.input_attach(ugfx.JOY_LEFT, input_other)
    ugfx.input_attach(ugfx.JOY_RIGHT, input_other)
    ugfx.input_attach(ugfx.BTN_START, input_other)

    populate_apps()
    populate_category()
    populate_options()

    ugfx.flush(ugfx.LUT_FULL)

start()
init_power_management()
