/*
 * This file is part of the Micro Python project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "py/nlr.h"
#include "py/parsenum.h"
#include "py/runtime0.h"
#include "py/runtime.h"

#if MICROPY_PY_BUILTINS_FLOAT

#include <math.h>
#include "py/formatfloat.h"

#if MICROPY_OBJ_REPR != MICROPY_OBJ_REPR_C && MICROPY_OBJ_REPR != MICROPY_OBJ_REPR_D

// M_E and M_PI are not part of the math.h standard and may not be defined
#ifndef M_E
#define M_E (2.7182818284590452354)
#endif
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

typedef struct _mp_obj_float_t {
    mp_obj_base_t base;
    mp_float_t value;
} mp_obj_float_t;

const mp_obj_float_t mp_const_float_e_obj = {{&mp_type_float}, M_E};
const mp_obj_float_t mp_const_float_pi_obj = {{&mp_type_float}, M_PI};

#endif

STATIC void float_print(const mp_print_t *print, mp_obj_t o_in, mp_print_kind_t kind) {
    (void)kind;
    mp_float_t o_val = mp_obj_float_get(o_in);
#if MICROPY_FLOAT_IMPL == MICROPY_FLOAT_IMPL_FLOAT
    char buf[16];
    const int precision = 7;
#else
    char buf[32];
    const int precision = 16;
#endif
    mp_format_float(o_val, buf, sizeof(buf), 'g', precision, '\0');
    mp_print_str(print, buf);
    if (strchr(buf, '.') == NULL && strchr(buf, 'e') == NULL && strchr(buf, 'n') == NULL) {
        // Python floats always have decimal point (unless inf or nan)
        mp_print_str(print, ".0");
    }
}

STATIC mp_obj_t float_make_new(const mp_obj_type_t *type_in, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    (void)type_in;
    mp_arg_check_num(n_args, n_kw, 0, 1, false);

    switch (n_args) {
        case 0:
            return mp_obj_new_float(0);

        case 1:
        default:
            if (MP_OBJ_IS_STR(args[0])) {
                // a string, parse it
                mp_uint_t l;
                const char *s = mp_obj_str_get_data(args[0], &l);
                return mp_parse_num_decimal(s, l, false, false, NULL);
            } else if (mp_obj_is_float(args[0])) {
                // a float, just return it
                return args[0];
            } else {
                // something else, try to cast it to a float
                return mp_obj_new_float(mp_obj_get_float(args[0]));
            }
    }
}

STATIC mp_obj_t float_unary_op(mp_uint_t op, mp_obj_t o_in) {
    mp_float_t val = mp_obj_float_get(o_in);
    switch (op) {
        case MP_UNARY_OP_BOOL: return mp_obj_new_bool(val != 0);
        case MP_UNARY_OP_POSITIVE: return o_in;
        case MP_UNARY_OP_NEGATIVE: return mp_obj_new_float(-val);
        default: return MP_OBJ_NULL; // op not supported
    }
}

STATIC mp_obj_t float_binary_op(mp_uint_t op, mp_obj_t lhs_in, mp_obj_t rhs_in) {
    mp_float_t lhs_val = mp_obj_float_get(lhs_in);
#if MICROPY_PY_BUILTINS_COMPLEX
    if (MP_OBJ_IS_TYPE(rhs_in, &mp_type_complex)) {
        return mp_obj_complex_binary_op(op, lhs_val, 0, rhs_in);
    } else
#endif
    {
        return mp_obj_float_binary_op(op, lhs_val, rhs_in);
    }
}

const mp_obj_type_t mp_type_float = {
    { &mp_type_type },
    .name = MP_QSTR_float,
    .print = float_print,
    .make_new = float_make_new,
    .unary_op = float_unary_op,
    .binary_op = float_binary_op,
};

#if MICROPY_OBJ_REPR != MICROPY_OBJ_REPR_C && MICROPY_OBJ_REPR != MICROPY_OBJ_REPR_D

mp_obj_t mp_obj_new_float(mp_float_t value) {
    mp_obj_float_t *o = m_new(mp_obj_float_t, 1);
    o->base.type = &mp_type_float;
    o->value = value;
    return MP_OBJ_FROM_PTR(o);
}

mp_float_t mp_obj_float_get(mp_obj_t self_in) {
    assert(mp_obj_is_float(self_in));
    mp_obj_float_t *self = MP_OBJ_TO_PTR(self_in);
    return self->value;
}

#endif

STATIC void mp_obj_float_divmod(mp_float_t *x, mp_float_t *y) {
    // logic here follows that of CPython
    // https://docs.python.org/3/reference/expressions.html#binary-arithmetic-operations
    // x == (x//y)*y + (x%y)
    // divmod(x, y) == (x//y, x%y)
    mp_float_t mod = MICROPY_FLOAT_C_FUN(fmod)(*x, *y);
    mp_float_t div = (*x - mod) / *y;

    // Python specs require that mod has same sign as second operand
    if (mod == 0.0) {
        mod = MICROPY_FLOAT_C_FUN(copysign)(0.0, *y);
    } else {
        if ((mod < 0.0) != (*y < 0.0)) {
            mod += *y;
            div -= 1.0;
        }
    }

    mp_float_t floordiv;
    if (div == 0.0) {
        // if division is zero, take the correct sign of zero
        floordiv = MICROPY_FLOAT_C_FUN(copysign)(0.0, *x / *y);
    } else {
        // Python specs require that x == (x//y)*y + (x%y)
        floordiv = MICROPY_FLOAT_C_FUN(floor)(div);
        if (div - floordiv > 0.5) {
            floordiv += 1.0;
        }
    }

    // return results
    *x = floordiv;
    *y = mod;
}

mp_obj_t mp_obj_float_binary_op(mp_uint_t op, mp_float_t lhs_val, mp_obj_t rhs_in) {
    mp_float_t rhs_val = mp_obj_get_float(rhs_in); // can be any type, this function will convert to float (if possible)
    switch (op) {
        case MP_BINARY_OP_ADD:
        case MP_BINARY_OP_INPLACE_ADD: lhs_val += rhs_val; break;
        case MP_BINARY_OP_SUBTRACT:
        case MP_BINARY_OP_INPLACE_SUBTRACT: lhs_val -= rhs_val; break;
        case MP_BINARY_OP_MULTIPLY:
        case MP_BINARY_OP_INPLACE_MULTIPLY: lhs_val *= rhs_val; break;
        case MP_BINARY_OP_FLOOR_DIVIDE:
        case MP_BINARY_OP_INPLACE_FLOOR_DIVIDE:
            if (rhs_val == 0) {
                zero_division_error:
                mp_raise_msg(&mp_type_ZeroDivisionError, "division by zero");
            }
            // Python specs require that x == (x//y)*y + (x%y) so we must
            // call divmod to compute the correct floor division, which
            // returns the floor divide in lhs_val.
            mp_obj_float_divmod(&lhs_val, &rhs_val);
            break;
        case MP_BINARY_OP_TRUE_DIVIDE:
        case MP_BINARY_OP_INPLACE_TRUE_DIVIDE:
            if (rhs_val == 0) {
                goto zero_division_error;
            }
            lhs_val /= rhs_val;
            break;
        case MP_BINARY_OP_MODULO:
        case MP_BINARY_OP_INPLACE_MODULO:
            if (rhs_val == 0) {
                goto zero_division_error;
            }
            lhs_val = MICROPY_FLOAT_C_FUN(fmod)(lhs_val, rhs_val);
            // Python specs require that mod has same sign as second operand
            if (lhs_val == 0.0) {
                lhs_val = MICROPY_FLOAT_C_FUN(copysign)(0.0, rhs_val);
            } else {
                if ((lhs_val < 0.0) != (rhs_val < 0.0)) {
                    lhs_val += rhs_val;
                }
            }
            break;
        case MP_BINARY_OP_POWER:
        case MP_BINARY_OP_INPLACE_POWER:
            if (lhs_val == 0 && rhs_val < 0) {
                goto zero_division_error;
            }
            lhs_val = MICROPY_FLOAT_C_FUN(pow)(lhs_val, rhs_val);
            break;
        case MP_BINARY_OP_DIVMOD: {
            if (rhs_val == 0) {
                goto zero_division_error;
            }
            mp_obj_float_divmod(&lhs_val, &rhs_val);
            mp_obj_t tuple[2] = {
                mp_obj_new_float(lhs_val),
                mp_obj_new_float(rhs_val),
            };
            return mp_obj_new_tuple(2, tuple);
        }
        case MP_BINARY_OP_LESS: return mp_obj_new_bool(lhs_val < rhs_val);
        case MP_BINARY_OP_MORE: return mp_obj_new_bool(lhs_val > rhs_val);
        case MP_BINARY_OP_EQUAL: return mp_obj_new_bool(lhs_val == rhs_val);
        case MP_BINARY_OP_LESS_EQUAL: return mp_obj_new_bool(lhs_val <= rhs_val);
        case MP_BINARY_OP_MORE_EQUAL: return mp_obj_new_bool(lhs_val >= rhs_val);

        default:
            return MP_OBJ_NULL; // op not supported
    }
    return mp_obj_new_float(lhs_val);
}

#endif // MICROPY_PY_BUILTINS_FLOAT
