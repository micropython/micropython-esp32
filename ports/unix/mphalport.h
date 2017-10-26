/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Damien P. George
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
#include <unistd.h>

#ifndef CHAR_CTRL_C
#define CHAR_CTRL_C (3)
#endif

void mp_hal_set_interrupt_char(char c);

void mp_hal_stdio_mode_raw(void);
void mp_hal_stdio_mode_orig(void);

#if MICROPY_USE_READLINE == 1 && MICROPY_PY_BUILTINS_INPUT
#include "py/misc.h"
#include "lib/mp-readline/readline.h"
// For built-in input() we need to wrap the standard readline() to enable raw mode
#define mp_hal_readline mp_hal_readline
static inline int mp_hal_readline(vstr_t *vstr, const char *p) {
    mp_hal_stdio_mode_raw();
    int ret = readline(vstr, p);
    mp_hal_stdio_mode_orig();
    return ret;
}
#endif

// TODO: POSIX et al. define usleep() as guaranteedly capable only of 1s sleep:
// "The useconds argument shall be less than one million."
static inline void mp_hal_delay_ms(mp_uint_t ms) { usleep((ms) * 1000); }
static inline void mp_hal_delay_us(mp_uint_t us) { usleep(us); }
#define mp_hal_ticks_cpu() 0

#define RAISE_ERRNO(err_flag, error_val) \
    { if (err_flag == -1) \
        { mp_raise_OSError(error_val); } }
