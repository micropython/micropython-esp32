from rgb import DisplaySPI, color565


_DRAWLINE = const(0x21)
_DRAWRECT = const(0x22)
_FILL = const(0x26)
_PHASEPERIOD = const(0x12)
_SETCOLUMN = const(0x15)
_SETROW = const(0x75)
_CONTRASTA = const(0x81)
_CONTRASTB = const(0x82)
_CONTRASTC = const(0x83)
_MASTERCURRENT = const(0x87)
_SETREMAP = const(0xA0)
_STARTLINE = const(0xA1)
_DISPLAYOFFSET = const(0xA2)
_NORMALDISPLAY = const(0xA4)
_DISPLAYALLON = const(0xA5)
_DISPLAYALLOFF = const(0xA6)
_INVERTDISPLAY = const(0xA7)
_SETMULTIPLEX = const(0xA8)
_SETMASTER = const(0xAD)
_DISPLAYOFF = const(0xAE)
_DISPLAYON = const(0xAF)
_POWERMODE = const(0xB0)
_PRECHARGE = const(0xB1)
_CLOCKDIV = const(0xB3)
_PRECHARGEA = const(0x8A)
_PRECHARGEB = const(0x8B)
_PRECHARGEC = const(0x8C)
_PRECHARGELEVEL = const(0xBB)
_VCOMH = const(0xBE)
_LOCK = const(0xfd)


class SSD1331(DisplaySPI):
    """
>>>
from machine import Pin, SPI
import ssd1331
#spi = SPI(mosi=Pin(13), sck=Pin(14), polarity=1, phase=1)
spi = SPI(1, polarity=1, phase=1)
display = ssd1331.SSD1331(spi, dc=Pin(12), cs=Pin(15), rst=Pin(16))
display.fill(0x7521)
display.pixel(32, 32, 0)
    """
    _COLUMN_SET = _SETCOLUMN
    _PAGE_SET = _SETROW
    _RAM_WRITE = None
    _RAM_READ = None
    _INIT = (
        (_DISPLAYOFF, b''),
        (_LOCK, b'\x0b'),
        (_SETREMAP, b'\x72'), # RGB Color
        (_STARTLINE, b'\x00'),
        (_DISPLAYOFFSET, b'\x00'),
        (_NORMALDISPLAY, b''),
#        (_FILL, b'\x01'),

#	(_PHASEPERIOD, b'\x31'),
#	(_SETMULTIPLEX, b'\x3f'),
#	(_SETMASTER, b'\x8e'),
#	(_POWERMODE,b'\x0b'),
#	(_PRECHARGE, b'\x31'), #;//0x1F - 0x31
#	(_CLOCKDIV, b'\xf0'),
#	(_VCOMH, b'\x3e'), #;//0x3E - 0x3F
#	(_MASTERCURRENT, b'\x06'), # ;//0x06 - 0x0F
#	(_PRECHARGEA, b'\x64'),
#	(_PRECHARGEB, b'\x78'),
#	(_PRECHARGEC, b'\x64'),
#	(_PRECHARGELEVEL, b'\x3a'), # 0x3A - 0x00
#	(_CONTRASTA, b'\x91'), #//0xEF - 0x91
#	(_CONTRASTB, b'\x50'), #;//0x11 - 0x50
#	(_CONTRASTC, b'\x7d'), #;//0x48 - 0x7D
        (_DISPLAYON, b''),
    )
    _ENCODE_PIXEL = ">H"
    _ENCODE_POS = ">BB"

    def __init__(self, spi, dc, cs, rst=None, width=96, height=64):
        super().__init__(spi, dc, cs, rst, width, height)

    def _write(self, command=None, data=None):
        if command is None:
            self.dc.high()
        else:
            self.dc.low()
        self.cs.low()
        if command is not None:
            self.spi.write(bytearray([command]))
        if data is not None:
            self.spi.write(data)
        self.cs.high()
