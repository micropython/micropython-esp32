"""
Micro Python driver for SD cards using esp-idf sd_emmc driver.

Example usage on ESP32:

    import sdcard, uos, esp
    sd = sdcard.SDCard(esp.SD_1LINE)
    vfs = uos.VfsFat(sd)
    uos.mount(vfs, '/sd')
    uos.chdir('/sd')
    uos.listdir()
    
If 'automount' is used

    import sdcard, uos
    sd = sdcard.SDCard(esp.SD_4LINE, True)
    uos.listdir()

"""

import esp

class SDCard:
    def __init__(self, mode, automount):
        self.SD_FOUND = esp.sdcard_init(mode)
        self.SEC_COUNT = esp.sdcard_sect_count()
        self.SEC_SIZE = esp.sdcard_sect_size()
        self.SIZE = self.SEC_SIZE * self.SEC_COUNT
        # automount sdcard if requested
        if self.SD_FOUND == 0 and automount:
            import uos
            vfs = uos.VfsFat(self)
            uos.mount(vfs, '/sd')
            uos.chdir('/sd')

    def count(self):
        return esp.sdcard_sect_count()

    def readblocks(self, block_num, buf):
        esp.sdcard_read(block_num, buf)
        return 0

    def writeblocks(self, block_num, buf):
        esp.sdcard_write(block_num, buf)
        return 0

