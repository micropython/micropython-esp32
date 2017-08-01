import gc, uos

try:
    uos.mount(uos.VfsNative(True), '/')
    open("/boot.py", "r")
except OSError:
    import inisetup
    vfs = inisetup.setup()

gc.collect()
