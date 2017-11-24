from rgb import DisplaySPI, color565

_SWRESET = const(0x01)
_DISPON = const(0x29)
_SLEEPOUT = const(0x11)
_CASET = const(0x2a)
_PASET = const(0x2b)
_RAMWR = const(0x2c)
_RAMRD = const(0x2e)
_COLMOD = const(0x3a)
_MADCTL = const(0x36)

class S6D02A1(DisplaySPI):
    """
    A simple driver for the S6D02A1-based displays.

    >>> from machine import Pin, SPI
    >>> import s6d02a1
    >>> spi = SPI(0, baudrate=40000000)
    >>> display = s6d02a1.S6D02A1(spi, dc=Pin(0), cs=Pin(15), rst=Pin(16))
    >>> display.fill(0x7521)
    >>> display.pixel(64, 64, 0)

    """
    _COLUMN_SET = _CASET
    _PAGE_SET = _PASET
    _RAM_WRITE = _RAMWR
    _RAM_READ = _RAMRD
    _INIT = (
        (_SWRESET, None),
        (_SLEEPOUT, None),
        (_MADCTL, b'\x10'), # bottom-top
        (_COLMOD, b'\x05'), # RGB565 pixel format
        (_DISPON, None),
    )
    _ENCODE_PIXEL = ">H"
    _ENCODE_POS = ">HH"

    def __init__(self, spi, dc, cs, rst=None, width=128, height=160):
        super().__init__(spi, dc, cs, rst, width, height)
