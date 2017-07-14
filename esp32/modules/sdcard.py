"""
Micro Python driver for SD cards using esp-idf sd_emmc driver.

Example usage on ESP32:

    import sdcard, uos, esp
    sd = sdcard.SDCard(False)
    vfs = uos.VfsFat(sd)
    uos.mount(vfs, '/sdcard')
    uos.chdir('/sdcard')
    uos.listdir()

If 'automount' is used

    import sdcard, uos
    sd = sdcard.SDCard(True)
    uos.listdir()

"""

import esp

class SDCard:
    def __init__(self, automount):
        self.SD_FOUND = esp.sdcard_init()
        self.SEC_COUNT = esp.sdcard_sect_count()
        self.SEC_SIZE = esp.sdcard_sect_size()
        self.SIZE = self.SEC_SIZE * self.SEC_COUNT
        # automount sdcard if requested
        if self.SD_FOUND == 0 and automount:
            import uos
            vfs = uos.VfsFat(self)
            uos.mount(vfs, '/sdcard')
            uos.chdir('/sdcard')

    def count(self):
        return esp.sdcard_sect_count()

    def readblocks(self, block_num, buf):
        esp.sdcard_read(block_num, buf)
        return 0

    def writeblocks(self, block_num, buf):
        esp.sdcard_write(block_num, buf)
        return 0
