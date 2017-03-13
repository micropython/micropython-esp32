/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Damien P. George
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

#include "py/mpconfig.h"
#if MICROPY_VFS_FAT

#include <string.h>
#include "py/nlr.h"
#include "py/runtime.h"
#include "lib/oofatfs/ff.h"
#include "extmod/vfs_fat.h"
#include "py/lexer.h"

// TODO: actually, the core function should be ilistdir()

mp_obj_t fat_vfs_listdir2(fs_user_mount_t *vfs, const char *path, bool is_str_type) {
    FRESULT res;
    FILINFO fno;
    FF_DIR dir;

    res = f_opendir(&vfs->fatfs, &dir, path);
    if (res != FR_OK) {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }

    mp_obj_t dir_list = mp_obj_new_list(0, NULL);

    for (;;) {
        res = f_readdir(&dir, &fno);                   /* Read a directory item */
        if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
        if (fno.fname[0] == '.' && fno.fname[1] == 0) continue;             /* Ignore . entry */
        if (fno.fname[0] == '.' && fno.fname[1] == '.' && fno.fname[2] == 0) continue;             /* Ignore .. entry */

        char *fn = fno.fname;

        /*
        if (fno.fattrib & AM_DIR) {
            // dir
        } else {
            // file
        }
        */

        // make a string object for this entry
        mp_obj_t entry_o;
        if (is_str_type) {
            entry_o = mp_obj_new_str(fn, strlen(fn), false);
        } else {
            entry_o = mp_obj_new_bytes((const byte*)fn, strlen(fn));
        }

        // add the entry to the list
        mp_obj_list_append(dir_list, entry_o);
    }

    f_closedir(&dir);

    return dir_list;
}

mp_import_stat_t fat_vfs_import_stat(fs_user_mount_t *vfs, const char *path) {
    FILINFO fno;
    assert(vfs != NULL);
    FRESULT res = f_stat(&vfs->fatfs, path, &fno);
    if (res == FR_OK) {
        if ((fno.fattrib & AM_DIR) != 0) {
            return MP_IMPORT_STAT_DIR;
        } else {
            return MP_IMPORT_STAT_FILE;
        }
    }
    return MP_IMPORT_STAT_NO_EXIST;
}

#endif // MICROPY_VFS_FAT
