from rgb import DisplaySPI, color565

_SWRESET = const(0x01)
_NORON = const(0x13)
_INVOFF = const(0x20)
_INVON = const(0x21)
_DISPOFF = const(0x28)
_DISPON = const(0x29)
_CASET = const(0x2a)
_PASET = const(0x2b)
_RAMWR = const(0x2c)
_RAMRD = const(0x2e)
_MADCTL = const(0x36)
_COLMOD = const(0x3a)

class HX8353(DisplaySPI):
    """
    A simple driver for the HX8383-based displays.

    >>> from machine import Pin, SPI
    >>> import hx8353
    >>> spi = SPI(0)
    >>> display = hx8353.HX8383(spi, dc=Pin(0), cs=Pin(15), rst=Pin(16))
    >>> display.fill(0x7521)
    >>> display.pixel(64, 64, 0)

    """
    _COLUMN_SET = _CASET
    _PAGE_SET = _PASET
    _RAM_WRITE = _RAMWR
    _RAM_READ = _RAMRD
    _INIT = (
        (_SWRESET, None),
        (_DISPON, None),
    )
    _ENCODE_PIXEL = ">H"
    _ENCODE_POS = ">HH"

    def __init__(self, spi, dc, cs, rst=None, width=128, height=128):
        super().__init__(spi, dc, cs, rst, width, height)
