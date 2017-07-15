import ugfx, badge, sys, gc
import uos as os
import uerrno as errno
import ujson as json
import time
import esp

ugfx.init()
ugfx.input_init()

ugfx.clear(ugfx.WHITE)
ugfx.string(180,25,"STILL","Roboto_BlackItalic24",ugfx.BLACK)
ugfx.string(160,50,"Hacking","PermanentMarker22",ugfx.BLACK)
str_len = ugfx.get_string_width("Hacking","PermanentMarker22")
ugfx.line(160, 72, 174 + str_len, 72, ugfx.BLACK)
ugfx.line(170 + str_len, 52, 170 + str_len, 70, ugfx.BLACK)
ugfx.string(170,75,"Anyway","Roboto_BlackItalic24",ugfx.BLACK)
ugfx.string(155,105,"MOTD: NVS","Roboto_Regular18",ugfx.BLACK)

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
        ugfx.string(40,25,"Running:","Roboto_BlackItalic24",ugfx.WHITE)
        ugfx.string(100,75, selected,"PermanentMarker22",ugfx.WHITE)
        ugfx.flush()
        badge.eink_busy_wait()
        esp.rtcmem_write_string(selected)
        esp.start_sleeping(1)

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
        import dialogs
        print(selected)
        uninstall = dialogs.prompt_boolean('Are you sure you want to remove %s?' % selected)
        print(uninstall)
        if uninstall:
            ugfx.clear(ugfx.BLACK)
            ugfx.string(40,25,"Uninstalling:","Roboto_BlackItalic24",ugfx.WHITE)
            ugfx.string(100,75, selected,"PermanentMarker22",ugfx.WHITE)
            ugfx.flush()
            install_path = get_install_path()
            for rm_file in os.listdir("%s/%s" % (install_path, selected)):
                os.remove("%s/%s/%s" % (install_path, selected, rm_file))
            os.rmdir("%s/%s" % (install_path, selected))
        badge.eink_busy_wait()
        esp.rtcmem_write_string('launcher')
        esp.start_sleeping(1)


populate_it()

ugfx.input_attach(ugfx.BTN_A, run_it)
ugfx.input_attach(ugfx.BTN_B, uninstall_it)

ugfx.input_attach(ugfx.JOY_UP, lambda pushed: ugfx.flush() if pushed else 0)
ugfx.input_attach(ugfx.JOY_DOWN, lambda pushed: ugfx.flush() if pushed else 0)

ugfx.set_lut(ugfx.LUT_FULL)
ugfx.flush()
ugfx.set_lut(ugfx.LUT_FASTER)
