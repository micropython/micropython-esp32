import ugfx, badge, sys, uos as os, appglue, version, easydraw, virtualtimers, tasks.powermanagement as pm

def populate_it():
    global options
    options = ugfx.List(0,0,int(ugfx.width()/2),ugfx.height())

    try:
        apps = os.listdir('lib')
    except OSError:
        apps = []

    options.add_item('installer')
    options.add_item('ota_update')

    for app in apps:
        if not app=="resources":
            options.add_item(app)
            
    options.add_item('setup')
        
def run_it():
    selected = options.selected_text()
    options.destroy()
    appglue.start_app(selected)

def expandhome(s):
    if "~/" in s:
        h = os.getenv("HOME")
        s = s.replace("~/", h + "/")
    return s

def gohome(pressed):
    if(pressed):
        appglue.home()

def get_install_path():
    global install_path
    if install_path is None:
        # sys.path[0] is current module's path
        install_path = sys.path[1]
    install_path = expandhome(install_path)
    return install_path

def uninstall_it():
    selected = options.selected_text()
    if selected == 'installer':
        return
    if selected == 'ota_update':
        return
    options.destroy()

    def perform_uninstall(ok):
        if ok:
            easydraw.msg(selected,"Uninstalling...",True)
            install_path = get_install_path()
            for rm_file in os.listdir("%s/%s" % (install_path, selected)):
                os.remove("%s/%s/%s" % (install_path, selected, rm_file))
            os.rmdir("%s/%s" % (install_path, selected))
        badge.eink_busy_wait()
        appglue.start_app('launcher')

    import dialogs
    uninstall = dialogs.prompt_boolean('Are you sure you want to remove %s?' % selected, cb=perform_uninstall)
        

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

# ugfx.flush()
options = None
install_path = None

populate_it()

def input_a(pressed):
    pm.feed()
    if pressed:
        run_it()
    
def input_b(pressed):
    pm.feed()
    if pressed:
        appglue.home()

def input_select(pressed):
    pm.feed()
    if pressed:
        uninstall_it()
    
def input_other(pressed):
    pm.feed()
    if pressed:
        ugfx.flush()

ugfx.input_attach(ugfx.BTN_A, input_a)
ugfx.input_attach(ugfx.BTN_B, input_b)
ugfx.input_attach(ugfx.BTN_SELECT, input_select)
ugfx.input_attach(ugfx.JOY_UP, input_other)
ugfx.input_attach(ugfx.JOY_DOWN, input_other)
ugfx.input_attach(ugfx.JOY_LEFT, input_other)
ugfx.input_attach(ugfx.JOY_RIGHT, input_other)
ugfx.input_attach(ugfx.BTN_START, input_other)

ugfx.flush(ugfx.LUT_FULL)

# Power management
virtualtimers.activate(1000) # Start scheduler with 1 second ticks
pm.set_timeout(5*60*1000) # Set timeout to 5 minutes
pm.callback(appglue.home()) # Go to splash instead of sleep
pm.feed() # Feed the power management task, starts the countdown...
