import badge, gc, uos

try:
    badge.mount_root()
    uos.mount(uos.VfsNative(None), '/')
    open("/boot.py", "r")
except OSError:
    import inisetup
    vfs = inisetup.setup()

gc.collect()
