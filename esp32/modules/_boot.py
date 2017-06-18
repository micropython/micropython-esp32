import gc
import uos
from flashbdev import bdev

try:
    if bdev:
        vfs = uos.VfsFat(bdev)
        uos.mount(vfs, '/flash')
        uos.chdir('/flash')
except OSError:
    import inisetup
    vfs = inisetup.setup()

gc.collect()
