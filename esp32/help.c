/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013-2016 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/builtin.h"

const char *esp32_help_text =
"Welcome to the SHA2017 badge!\n"
"This version of micropython was compiled on " __DATE__ " at " __TIME__ "\n"
"\n"
"For the project page please visit https://wiki.sha2017.org/w/Projects:Badge\n"
"\n"
"Basic WiFi configuration:\n"
"\n"
"import network\n"
"sta_if = network.WLAN(network.STA_IF); sta_if.active(True)       # Activate standalone interface\n"
"sta_if.scan()                                                    # Scan for available access points\n"
"sta_if.connect(\"SHA2017_public\")                                 # Connect to the public SHA2017 AP without a password\n"
"sta_if.isconnected()                                             # Check for successful connection\n"
"sta_if.ifconfig()                                                # Print connection information\n"
"\n"
"Using the e-ink display:\n"
"\n"
"import ugfx\n"
"ugfx.init()                                                      # Initialize display\n"
"ugfx.clear(ugfx.BLACK)                                           # Clear screen\n"
"ugfx.string(120, 50, \"Test\", \"Roboto_BlackItalic24\", ugfx.WHITE) # Write a string to the center of the screen\n"
"ugfx.flush()                                                     # Send the update to the screen\n"
"\n"
"Interfacing with the buttons:\n"
"\n"
"import ugfx\n"
"ugfx.init()                                                      # Initialize ugfx subsystem\n"
"ugfx.input_init()                                                # Initialize button callbacks\n"
"ugfx.input_attach(ugfx.JOY_UP, lambda pressed: print(pressed))   # Connect button callback function\n"
"while True: pass                                                 # Stay in python loop to get events\n"
"\n"
"Controlling the RGBW LEDs:\n"
"\n"
"import badge\n"
"badge.send_led_data([r1,g1,b1,w1, r2,g2,b2,w2, r3,g3,b3,w3, ...])\n"
"\n"
"For further help on a specific object, type help(obj)\n"
"For a list of available modules, type help('modules')\n"
;
