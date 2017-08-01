#include "py/mpconfig.h"
#if MICROPY_VFS_NATIVE

#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <esp_log.h>

#include "py/nlr.h"
#include "py/runtime.h"
#include "extmod/vfs_native.h"
#include "py/lexer.h"

static const char *TAG = "vfs_native_misc.c";

typedef struct _mp_vfs_native_ilistdir_it_t {
	mp_obj_base_t base;
	mp_fun_1_t iternext;
	bool is_str;
	DIR *dir;
} mp_vfs_native_ilistdir_it_t;

STATIC mp_obj_t mp_vfs_native_ilistdir_it_iternext(mp_obj_t self_in) {
	mp_vfs_native_ilistdir_it_t *self = MP_OBJ_TO_PTR(self_in);

	for (;;) {
		struct dirent *de;
		de = readdir(self->dir);
		if (de == NULL) {
			// stop on error or end of dir
			break;
		}

		char *fn = de->d_name;
		ESP_LOGD(TAG, "readdir -> '%s'", fn);

		// filter . and ..
		if (fn[0] == '.' && ((fn[1] == '.' && fn[2] == 0) || fn[1] == 0))
			continue;

		// make 3-tuple with info about this entry
		mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(3, NULL));
		if (self->is_str) {
			t->items[0] = mp_obj_new_str(fn, strlen(fn), false);
		} else {
			t->items[0] = mp_obj_new_bytes((const byte*)fn, strlen(fn));
		}
		if (de->d_type & DT_DIR) {
			// dir
			t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFDIR);
		} else {
			// file
			t->items[1] = MP_OBJ_NEW_SMALL_INT(MP_S_IFREG);
		}
		t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // no inode number

		return MP_OBJ_FROM_PTR(t);
	}

	// ignore error because we may be closing a second time
	closedir(self->dir);

	return MP_OBJ_STOP_ITERATION;
}

mp_obj_t native_vfs_ilistdir2(fs_user_mount_t *vfs, const char *path, bool is_str_type) {
	mp_vfs_native_ilistdir_it_t *iter = m_new_obj(mp_vfs_native_ilistdir_it_t);
	iter->base.type = &mp_type_polymorph_iter;
	iter->iternext = mp_vfs_native_ilistdir_it_iternext;
	iter->is_str = is_str_type;

	ESP_LOGD(TAG, "opendir('%s')", path);

	DIR *d = opendir(path);
	if (d == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}
	iter->dir = d;
	return MP_OBJ_FROM_PTR(iter);
}

mp_import_stat_t native_vfs_import_stat(fs_user_mount_t *vfs, const char *path) {
	char absbuf[MICROPY_ALLOC_PATH_MAX + 1];
	path = mkabspath(path, absbuf, sizeof(absbuf));
	if (path == NULL) {
		return MP_IMPORT_STAT_NO_EXIST;
	}

	struct stat buf;
	ESP_LOGD(TAG, "import_stat('%s')", path);
	int res = stat(path, &buf);
	if (res < 0) {
		return MP_IMPORT_STAT_NO_EXIST;
	}
	if ((buf.st_mode & S_IFDIR) == 0) {
		return MP_IMPORT_STAT_FILE;
	}
	return MP_IMPORT_STAT_DIR;
}

#endif // MICROPY_VFS_NATIVE
