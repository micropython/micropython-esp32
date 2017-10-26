/*
 * This file is part of the MicroPython project, http://micropython.org/
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

// Default unix config while intended to be comprehensive, may still not enable
// all the features, this config should enable more (testable) options.

#define MICROPY_VFS                    (1)
#define MICROPY_PY_UOS_VFS             (1)

#include <mpconfigport.h>

#define MICROPY_FLOAT_HIGH_QUALITY_HASH (1)
#define MICROPY_ENABLE_SCHEDULER       (1)
#define MICROPY_PY_DELATTR_SETATTR     (1)
#define MICROPY_PY_BUILTINS_HELP       (1)
#define MICROPY_PY_BUILTINS_HELP_MODULES (1)
#define MICROPY_PY_SYS_GETSIZEOF       (1)
#define MICROPY_PY_URANDOM_EXTRA_FUNCS (1)
#define MICROPY_PY_IO_BUFFEREDWRITER (1)
#undef MICROPY_VFS_FAT
#define MICROPY_VFS_FAT                (1)
#define MICROPY_PY_FRAMEBUF            (1)
