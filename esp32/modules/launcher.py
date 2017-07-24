import ugfx, badge, sys, gc
import uos as os
import uerrno as errno
import ujson as json
import time
import esp
import appglue
import version

ugfx.init()
ugfx.input_init()
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
ugfx.string_box(148,78,148,18, " B: Uninstall", "Roboto_Regular12", ugfx.BLACK, ugfx.justifyRight)
ugfx.string_box(148,92,148,18, " START: Return to home", "Roboto_Regular12", ugfx.BLACK, ugfx.justifyLeft)
ugfx.line(148, 110, 296, 110, ugfx.BLACK)
ugfx.string_box(148,110,148,18, " " + version.name, "Roboto_Regular12", ugfx.BLACK, ugfx.justifyLeft)

# ugfx.flush()
options = None
install_path = None

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
        options.add_item(app)

def run_it(pushed):
    if (pushed):
        selected = options.selected_text()
        options.destroy()

        ugfx.clear(ugfx.BLACK)
        ugfx.string_box(0, 25, 296, 25,"Running:","Roboto_BlackItalic24",ugfx.WHITE, ugfx.justifyCenter)
        ugfx.string_box(0, 51, 296, 23, selected, "PermanentMarker22", ugfx.WHITE, ugfx.justifyCenter)
        ugfx.flush()
        badge.eink_busy_wait()
        appglue.start_app(selected)

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

def uninstall_it(pushed):
    if (pushed):
        selected = options.selected_text()
        if selected == 'installer':
            return
        if selected == 'ota_update':
            return
        options.destroy()

        def perform_uninstall(ok):
            if ok:
                ugfx.clear(ugfx.BLACK)
                ugfx.string_box(0, 25, 296, 25,"Uninstalling:","Roboto_BlackItalic24",ugfx.WHITE, ugfx.justifyCenter)
                ugfx.string_box(0, 51, 296, 23, selected, "PermanentMarker22", ugfx.WHITE, ugfx.justifyCenter)
                ugfx.flush()
                install_path = get_install_path()
                for rm_file in os.listdir("%s/%s" % (install_path, selected)):
                    os.remove("%s/%s/%s" % (install_path, selected, rm_file))
                os.rmdir("%s/%s" % (install_path, selected))
            badge.eink_busy_wait()
            appglue.start_app('launcher')

        import dialogs
        uninstall = dialogs.prompt_boolean('Are you sure you want to remove %s?' % selected, cb=perform_uninstall)

populate_it()

ugfx.input_attach(ugfx.BTN_A, run_it)
ugfx.input_attach(ugfx.BTN_B, uninstall_it)

ugfx.input_attach(ugfx.JOY_UP, lambda pushed: ugfx.flush() if pushed else 0)
ugfx.input_attach(ugfx.JOY_DOWN, lambda pushed: ugfx.flush() if pushed else 0)

ugfx.input_attach(ugfx.BTN_START, lambda pushed: appglue.start_app("") if pushed else 0)

ugfx.set_lut(ugfx.LUT_FULL)
ugfx.flush()
ugfx.set_lut(ugfx.LUT_FASTER)
