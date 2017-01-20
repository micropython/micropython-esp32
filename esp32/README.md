MicroPython port to the ESP32
=============================

This is an experimental port of MicroPython to the Espressif ESP32
microcontroller.  It uses the ESP-IDF framework and MicroPython runs as
a task under FreeRTOS.

Supported features include:
- REPL (Python prompt) over UART0.
- 16k stack for the MicroPython task and 64k Python heap.
- Many of MicroPython's features are enabled: unicode, arbitrary-precision
  integers, single-precision floats, complex numbers, frozen bytecode, as
  well as many of the internal modules.
- Internal filesystem using the flash (currently 256k in size).
- The machine module with basic GPIO and bit-banging I2C, SPI support.

Development of this ESP32 port was sponsored in part by Microbric Pty Ltd.

Setting up the toolchain and ESP-IDF
------------------------------------

There are two main components that are needed to build the firmware:
- the Xtensa cross-compiler that targets the CPU in the ESP32 (this is
  different to the compiler used by the ESP8266)
- the Espressif IDF (IoT development framework, aka SDK)

Instructions for setting up both of these components are provided by the
ESP-IDF itself, which is found at https://github.com/espressif/esp-idf .
Follow the guide "Setting Up ESP-IDF", for Windows, Mac or Linux.  You
only need to perform up to "Step 2" of the guide, by which stage you
should have installed the cross-compile and cloned the ESP-IDF repository.

Once everything is set up you should have a functioning toolchain with
prefix xtensa-esp32-elf- (or otherwise if you configured it differently)
as well as a copy of the ESP-IDF repository.

You then need to set the `ESPIDF` environment/makefile variable to point to
the root of the ESP-IDF repository.  You can set the variable in your PATH,
or at the command line when calling make, or in your own custom `makefile`.
The last option is recommended as it allows you to easily configure other
variables for the build.  In that case, create a new file in the esp32
directory called `makefile` and add the following lines to that file:
```
ESPIDF = <path to root of esp-idf repository>
#PORT = /dev/ttyUSB0
#FLASH_MODE = qio
#FLASH_SIZE = 4MB
#CROSS_COMPILE = xtensa-esp32-elf-

include Makefile
```
Be sure to enter the correct path to your local copy of the IDF repository
(and use `$(HOME)`, not tilde, to reference your home directory).
If the Xtensa cross-compiler is not in your path you can use the
`CROSS_COMPILE` variable to set its location.  Other options of interest
are `PORT` for the serial port of your esp32 module, and `FLASH_MODE`
(which may need to be `dio` for some modules)
and `FLASH_SIZE`.  See the Makefile for further information.

Building the firmware
---------------------

The MicroPython cross-compiler must be built to pre-compile some of the
built-in scripts to bytecode.  This can be done by (from the root of
this repository):
```bash
$ make -C mpy-cross
```

Then to build MicroPython for the ESP32 run:
```bash
$ cd esp32
$ make
```
This will produce binary firmware images in the `build/` subdirectory
(three of them: bootloader.bin, partitions.bin and application.bin).

To flash the firmware you must have your ESP32 module in the bootloader
mode and connected to a serial port on your PC.  Refer to the documentation
for your particular ESP32 module for how to do this.  The serial port and
flash settings are set in the `Makefile`, and can be overridden in your
local `makefile`; see above for more details.

If you are installing MicroPython to your module for the first time, or
after installing any other firmware, you should first erase the flash
completely:
```bash
$ make erase
```

To flash the MicroPython firmware to your ESP32 use:
```bash
$ make deploy
```
This will use the `esptool.py` script (provided by ESP-IDF) to download the
binary images.

Getting a Python prompt
-----------------------

You can get a prompt via the serial port, via UART0, which is the same UART
that is used for programming the firmware.  The baudrate for the REPL is
115200 and you can use a command such as:
```bash
$ picocom /dev/ttyUSB0
```
