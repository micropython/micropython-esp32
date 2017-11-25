from distutils.core import setup


setup(
    name='adafruit-rgb-display',
    py_modules=[
        'rgb',
        'hx8353',
        'ili9341',
        's6d02a1',
        'ssd1331',
        'ssd1351',
        'st7735',
    ],
    version="1.0.1",
    description="Drivers for RGB displays for MicroPython.",
    long_description="""\
This library lets you use a number of popular RGB displays with MicroPython.""",
    author='Radomir Dopieralski',
    author_email='micropython@sheep.art.pl',
    classifiers = [
        'Programming Language :: Python :: 3',
        'License :: OSI Approved :: MIT License',
    ],
    url="https://bitbucket.org/thesheep/micropython-rgb",
)
