/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Damien P. George
 * Copyright (c) 2016 Paul Sokolovsky
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

#if !MICROPY_VFS
#error "with MICROPY_VFS_FAT enabled, must also enable MICROPY_VFS"
#endif

#include <string.h>
#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "lib/oofatfs/ff.h"
#include "extmod/vfs_fat.h"
#include "lib/timeutils/timeutils.h"

#if _MAX_SS == _MIN_SS
#define SECSIZE(fs) (_MIN_SS)
#else
#define SECSIZE(fs) ((fs)->ssize)
#endif

#define mp_obj_fat_vfs_t fs_user_mount_t

STATIC mp_obj_t fat_vfs_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 1, 1, false);

    // create new object
    fs_user_mount_t *vfs = m_new_obj(fs_user_mount_t);
    vfs->base.type = type;
    vfs->flags = FSUSER_FREE_OBJ;
    vfs->fatfs.drv = vfs;

    // load block protocol methods
    mp_load_method(args[0], MP_QSTR_readblocks, vfs->readblocks);
    mp_load_method_maybe(args[0], MP_QSTR_writeblocks, vfs->writeblocks);
    mp_load_method_maybe(args[0], MP_QSTR_ioctl, vfs->u.ioctl);
    if (vfs->u.ioctl[0] != MP_OBJ_NULL) {
        // device supports new block protocol, so indicate it
        vfs->flags |= FSUSER_HAVE_IOCTL;
    } else {
        // no ioctl method, so assume the device uses the old block protocol
        mp_load_method_maybe(args[0], MP_QSTR_sync, vfs->u.old.sync);
        mp_load_method(args[0], MP_QSTR_count, vfs->u.old.count);
    }

    return MP_OBJ_FROM_PTR(vfs);
}

STATIC mp_obj_t fat_vfs_mkfs(mp_obj_t bdev_in) {
    // create new object
    fs_user_mount_t *vfs = MP_OBJ_TO_PTR(fat_vfs_make_new(&mp_fat_vfs_type, 1, 0, &bdev_in));

    // make the filesystem
    uint8_t working_buf[_MAX_SS];
    FRESULT res = f_mkfs(&vfs->fatfs, FM_FAT | FM_SFD, 0, working_buf, sizeof(working_buf));
    if (res != FR_OK) {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(fat_vfs_mkfs_fun_obj, fat_vfs_mkfs);
STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(fat_vfs_mkfs_obj, MP_ROM_PTR(&fat_vfs_mkfs_fun_obj));

STATIC MP_DEFINE_CONST_FUN_OBJ_3(fat_vfs_open_obj, fatfs_builtin_open_self);

STATIC mp_obj_t fat_vfs_ilistdir_func(size_t n_args, const mp_obj_t *args) {
    mp_obj_fat_vfs_t *self = MP_OBJ_TO_PTR(args[0]);
    bool is_str_type = true;
    const char *path;
    if (n_args == 2) {
        if (mp_obj_get_type(args[1]) == &mp_type_bytes) {
            is_str_type = false;
        }
        path = mp_obj_str_get_str(args[1]);
    } else {
        path = "";
    }

    return fat_vfs_ilistdir2(self, path, is_str_type);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(fat_vfs_ilistdir_obj, 1, 2, fat_vfs_ilistdir_func);

STATIC mp_obj_t fat_vfs_remove_internal(mp_obj_t vfs_in, mp_obj_t path_in, mp_int_t attr) {
    mp_obj_fat_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path = mp_obj_str_get_str(path_in);

    FILINFO fno;
    FRESULT res = f_stat(&self->fatfs, path, &fno);

    if (res != FR_OK) {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }

    // check if path is a file or directory
    if ((fno.fattrib & AM_DIR) == attr) {
        res = f_unlink(&self->fatfs, path);

        if (res != FR_OK) {
            mp_raise_OSError(fresult_to_errno_table[res]);
        }
        return mp_const_none;
    } else {
        mp_raise_OSError(attr ? MP_ENOTDIR : MP_EISDIR);
    }
}

STATIC mp_obj_t fat_vfs_remove(mp_obj_t vfs_in, mp_obj_t path_in) {
    return fat_vfs_remove_internal(vfs_in, path_in, 0); // 0 == file attribute
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(fat_vfs_remove_obj, fat_vfs_remove);

STATIC mp_obj_t fat_vfs_rmdir(mp_obj_t vfs_in, mp_obj_t path_in) {
    return fat_vfs_remove_internal(vfs_in, path_in, AM_DIR);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(fat_vfs_rmdir_obj, fat_vfs_rmdir);

STATIC mp_obj_t fat_vfs_rename(mp_obj_t vfs_in, mp_obj_t path_in, mp_obj_t path_out) {
    mp_obj_fat_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *old_path = mp_obj_str_get_str(path_in);
    const char *new_path = mp_obj_str_get_str(path_out);
    FRESULT res = f_rename(&self->fatfs, old_path, new_path);
    if (res == FR_EXIST) {
        // if new_path exists then try removing it (but only if it's a file)
        fat_vfs_remove_internal(vfs_in, path_out, 0); // 0 == file attribute
        // try to rename again
        res = f_rename(&self->fatfs, old_path, new_path);
    }
    if (res == FR_OK) {
        return mp_const_none;
    } else {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }

}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(fat_vfs_rename_obj, fat_vfs_rename);

STATIC mp_obj_t fat_vfs_mkdir(mp_obj_t vfs_in, mp_obj_t path_o) {
    mp_obj_fat_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path = mp_obj_str_get_str(path_o);
    FRESULT res = f_mkdir(&self->fatfs, path);
    if (res == FR_OK) {
        return mp_const_none;
    } else {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(fat_vfs_mkdir_obj, fat_vfs_mkdir);

/// Change current directory.
STATIC mp_obj_t fat_vfs_chdir(mp_obj_t vfs_in, mp_obj_t path_in) {
    mp_obj_fat_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path;
    path = mp_obj_str_get_str(path_in);

    FRESULT res = f_chdir(&self->fatfs, path);

    if (res != FR_OK) {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(fat_vfs_chdir_obj, fat_vfs_chdir);

/// Get the current directory.
STATIC mp_obj_t fat_vfs_getcwd(mp_obj_t vfs_in) {
    mp_obj_fat_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    char buf[MICROPY_ALLOC_PATH_MAX + 1];
    FRESULT res = f_getcwd(&self->fatfs, buf, sizeof(buf));
    if (res != FR_OK) {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }
    return mp_obj_new_str(buf, strlen(buf), false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(fat_vfs_getcwd_obj, fat_vfs_getcwd);

/// \function stat(path)
/// Get the status of a file or directory.
STATIC mp_obj_t fat_vfs_stat(mp_obj_t vfs_in, mp_obj_t path_in) {
    mp_obj_fat_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    const char *path = mp_obj_str_get_str(path_in);

    FILINFO fno;
    if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
        // stat root directory
        fno.fsize = 0;
        fno.fdate = 0x2821; // Jan 1, 2000
        fno.ftime = 0;
        fno.fattrib = AM_DIR;
    } else {
        FRESULT res = f_stat(&self->fatfs, path, &fno);
        if (res != FR_OK) {
            mp_raise_OSError(fresult_to_errno_table[res]);
        }
    }

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));
    mp_int_t mode = 0;
    if (fno.fattrib & AM_DIR) {
        mode |= MP_S_IFDIR;
    } else {
        mode |= MP_S_IFREG;
    }
    mp_int_t seconds = timeutils_seconds_since_2000(
        1980 + ((fno.fdate >> 9) & 0x7f),
        (fno.fdate >> 5) & 0x0f,
        fno.fdate & 0x1f,
        (fno.ftime >> 11) & 0x1f,
        (fno.ftime >> 5) & 0x3f,
        2 * (fno.ftime & 0x1f)
    );
    t->items[0] = MP_OBJ_NEW_SMALL_INT(mode); // st_mode
    t->items[1] = MP_OBJ_NEW_SMALL_INT(0); // st_ino
    t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // st_dev
    t->items[3] = MP_OBJ_NEW_SMALL_INT(0); // st_nlink
    t->items[4] = MP_OBJ_NEW_SMALL_INT(0); // st_uid
    t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // st_gid
    t->items[6] = mp_obj_new_int_from_uint(fno.fsize); // st_size
    t->items[7] = MP_OBJ_NEW_SMALL_INT(seconds); // st_atime
    t->items[8] = MP_OBJ_NEW_SMALL_INT(seconds); // st_mtime
    t->items[9] = MP_OBJ_NEW_SMALL_INT(seconds); // st_ctime

    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(fat_vfs_stat_obj, fat_vfs_stat);

// Get the status of a VFS.
STATIC mp_obj_t fat_vfs_statvfs(mp_obj_t vfs_in, mp_obj_t path_in) {
    mp_obj_fat_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
    (void)path_in;

    DWORD nclst;
    FATFS *fatfs = &self->fatfs;
    FRESULT res = f_getfree(fatfs, &nclst);
    if (FR_OK != res) {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }

    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));

    t->items[0] = MP_OBJ_NEW_SMALL_INT(fatfs->csize * SECSIZE(fatfs)); // f_bsize
    t->items[1] = t->items[0]; // f_frsize
    t->items[2] = MP_OBJ_NEW_SMALL_INT((fatfs->n_fatent - 2)); // f_blocks
    t->items[3] = MP_OBJ_NEW_SMALL_INT(nclst); // f_bfree
    t->items[4] = t->items[3]; // f_bavail
    t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // f_files
    t->items[6] = MP_OBJ_NEW_SMALL_INT(0); // f_ffree
    t->items[7] = MP_OBJ_NEW_SMALL_INT(0); // f_favail
    t->items[8] = MP_OBJ_NEW_SMALL_INT(0); // f_flags
    t->items[9] = MP_OBJ_NEW_SMALL_INT(_MAX_LFN); // f_namemax

    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(fat_vfs_statvfs_obj, fat_vfs_statvfs);

STATIC mp_obj_t vfs_fat_mount(mp_obj_t self_in, mp_obj_t readonly, mp_obj_t mkfs) {
    fs_user_mount_t *self = MP_OBJ_TO_PTR(self_in);

    // Read-only device indicated by writeblocks[0] == MP_OBJ_NULL.
    // User can specify read-only device by:
    //  1. readonly=True keyword argument
    //  2. nonexistent writeblocks method (then writeblocks[0] == MP_OBJ_NULL already)
    if (mp_obj_is_true(readonly)) {
        self->writeblocks[0] = MP_OBJ_NULL;
    }

    // mount the block device
    FRESULT res = f_mount(&self->fatfs);

    // check if we need to make the filesystem
    if (res == FR_NO_FILESYSTEM && mp_obj_is_true(mkfs)) {
        uint8_t working_buf[_MAX_SS];
        res = f_mkfs(&self->fatfs, FM_FAT | FM_SFD, 0, working_buf, sizeof(working_buf));
    }
    if (res != FR_OK) {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(vfs_fat_mount_obj, vfs_fat_mount);

STATIC mp_obj_t vfs_fat_umount(mp_obj_t self_in) {
    fs_user_mount_t *self = MP_OBJ_TO_PTR(self_in);
    FRESULT res = f_umount(&self->fatfs);
    if (res != FR_OK) {
        mp_raise_OSError(fresult_to_errno_table[res]);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(fat_vfs_umount_obj, vfs_fat_umount);

STATIC const mp_rom_map_elem_t fat_vfs_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_mkfs), MP_ROM_PTR(&fat_vfs_mkfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&fat_vfs_open_obj) },
    { MP_ROM_QSTR(MP_QSTR_ilistdir), MP_ROM_PTR(&fat_vfs_ilistdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_mkdir), MP_ROM_PTR(&fat_vfs_mkdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_rmdir), MP_ROM_PTR(&fat_vfs_rmdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_chdir), MP_ROM_PTR(&fat_vfs_chdir_obj) },
    { MP_ROM_QSTR(MP_QSTR_getcwd), MP_ROM_PTR(&fat_vfs_getcwd_obj) },
    { MP_ROM_QSTR(MP_QSTR_remove), MP_ROM_PTR(&fat_vfs_remove_obj) },
    { MP_ROM_QSTR(MP_QSTR_rename), MP_ROM_PTR(&fat_vfs_rename_obj) },
    { MP_ROM_QSTR(MP_QSTR_stat), MP_ROM_PTR(&fat_vfs_stat_obj) },
    { MP_ROM_QSTR(MP_QSTR_statvfs), MP_ROM_PTR(&fat_vfs_statvfs_obj) },
    { MP_ROM_QSTR(MP_QSTR_mount), MP_ROM_PTR(&vfs_fat_mount_obj) },
    { MP_ROM_QSTR(MP_QSTR_umount), MP_ROM_PTR(&fat_vfs_umount_obj) },
};
STATIC MP_DEFINE_CONST_DICT(fat_vfs_locals_dict, fat_vfs_locals_dict_table);

const mp_obj_type_t mp_fat_vfs_type = {
    { &mp_type_type },
    .name = MP_QSTR_VfsFat,
    .make_new = fat_vfs_make_new,
    .locals_dict = (mp_obj_dict_t*)&fat_vfs_locals_dict,
};

#endif // MICROPY_VFS_FAT
