#include "py/mpconfig.h"
#if MICROPY_VFS_NATIVE

#if !MICROPY_VFS
#error "with MICROPY_VFS_NATIVE enabled, must also enable MICROPY_VFS"
#endif

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "py/nlr.h"
#include "py/runtime.h"
#include "py/mperrno.h"
#include "extmod/vfs_native.h"
#include "lib/timeutils/timeutils.h"

#include "esp_vfs.h"
#include "src/esp_vfs_fat.h"
#include "esp_system.h"

#define mp_obj_native_vfs_t fs_user_mount_t

static const char *TAG = "vfs_native.c";

static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;
static bool native_vfs_mounted = false;

/* esp-idf doesn't seem to have a cwd; create one. */
char cwd[MICROPY_ALLOC_PATH_MAX + 1] = { 0 };

int
chdir(const char *path)
{
	struct stat buf;
	int res = stat(path, &buf);
	if (res < 0) {
		return -1;
	}
	if ((buf.st_mode & S_IFDIR) == 0)
	{
		errno = ENOTDIR;
		return -1;
	}
	if (strlen(path) >= sizeof(cwd))
	{
		errno = ENAMETOOLONG;
		return -1;
	}

	strncpy(cwd, path, sizeof(cwd));
	ESP_LOGD(TAG, "cwd set to '%s'", cwd);
	return 0;
}
char *
getcwd(char *buf, size_t size)
{
	ESP_LOGD(TAG, "requesting cwd '%s'", cwd);
	if (size <= strlen(cwd))
	{
		errno = ENAMETOOLONG;
		return NULL;
	}
	strcpy(buf, cwd);
	return buf;
}
const char *
mkabspath(const char *path)
{
	ESP_LOGV(TAG, "abspath '%s' in cwd '%s'", path, cwd);
	// path is already absolute
	if (path[0] == '/')
	{
		ESP_LOGV(TAG, " `-> '%s'", path);
		return path;
	}

	// use two buffers to support methods like rename()
	static bool altbuf = false;
	static char buf1[MICROPY_ALLOC_PATH_MAX + 1];
	static char buf2[MICROPY_ALLOC_PATH_MAX + 1];
	char *buf = altbuf ? buf1 : buf2;
	altbuf = !altbuf;

	strcpy(buf, cwd);
	int len = strlen(buf);
	while (1) {
		// handle './' and '../'
		if (path[0] == 0)
			break;
		if (path[0] == '.' && path[1] == 0) { // '.'
			path = &path[1];
			break;
		}
		if (path[0] == '.' && path[1] == '/') { // './'
			path = &path[2];
			continue;
		}
		if (path[0] == '.' && path[1] == '.' && path[2] == 0) { // '..'
			path = &path[2];
			while (len > 0 && buf[len] != '/') len--;
			buf[len] = 0;
			break;
		}
		if (path[0] == '.' && path[1] == '.' && path[2] == '/') { // '../'
			path = &path[3];
			while (len > 0 && buf[len] != '/') len--;
			buf[len] = 0;
			continue;
		}
		if (strlen(buf) >= sizeof(buf1)-1) {
			errno = ENAMETOOLONG;
			return NULL;
		}
		strcat(buf, "/");
		len++;
		break;
	}

	if (strlen(buf) + strlen(path) >= sizeof(buf1)) {
		errno = ENAMETOOLONG;
		return NULL;
	}
	strcat(buf, path);
	if (buf[0] == 0) {
		strcat(buf, "/");
	}
	ESP_LOGV(TAG, " `-> '%s'", buf);
	return buf;
}

STATIC mp_obj_t native_vfs_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
	mp_arg_check_num(n_args, n_kw, 1, 1, false);

	// create new object
	ESP_LOGD(TAG, "new()");
	fs_user_mount_t *vfs = m_new_obj(fs_user_mount_t);
	vfs->base.type = type;

	return MP_OBJ_FROM_PTR(vfs);
}

STATIC mp_obj_t native_vfs_mkfs(mp_obj_t bdev_in) {
	ESP_LOGD(TAG, "mkfs()");
	// not supported
	mp_raise_OSError(ENOENT);
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(native_vfs_mkfs_fun_obj, native_vfs_mkfs);
STATIC MP_DEFINE_CONST_STATICMETHOD_OBJ(native_vfs_mkfs_obj, MP_ROM_PTR(&native_vfs_mkfs_fun_obj));

STATIC MP_DEFINE_CONST_FUN_OBJ_3(native_vfs_open_obj, nativefs_builtin_open_self);

STATIC mp_obj_t native_vfs_ilistdir_func(size_t n_args, const mp_obj_t *args) {
	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(args[0]);

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

	path = mkabspath(path);
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	ESP_LOGD(TAG, "ilistdir('%s')", path);
	return native_vfs_ilistdir2(self, path, is_str_type);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(native_vfs_ilistdir_obj, 1, 2, native_vfs_ilistdir_func);

STATIC mp_obj_t native_vfs_remove(mp_obj_t vfs_in, mp_obj_t path_in) {
//	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	path = mkabspath(path);
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	ESP_LOGD(TAG, "unlink('%s')", path);
	int res = unlink(path);
	if (res < 0) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_remove_obj, native_vfs_remove);

STATIC mp_obj_t native_vfs_rmdir(mp_obj_t vfs_in, mp_obj_t path_in) {
//	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	path = mkabspath(path);
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	ESP_LOGD(TAG, "rmdir('%s')", path);
	int res = rmdir(path);
	if (res < 0) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_rmdir_obj, native_vfs_rmdir);

STATIC mp_obj_t native_vfs_rename(mp_obj_t vfs_in, mp_obj_t path_in, mp_obj_t path_out) {
//	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *old_path = mp_obj_str_get_str(path_in);
	const char *new_path = mp_obj_str_get_str(path_out);

	old_path = mkabspath(old_path);
	if (old_path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	new_path = mkabspath(new_path);
	if (new_path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	ESP_LOGD(TAG, "rename('%s', '%s')", old_path, new_path);
	int res = rename(old_path, new_path);
	/*
	// FIXME: have to check if we can replace files with this
	if (res < 0 && errno == EEXISTS) {
		res = unlink(new_path);
		if (res < 0) {
			mp_raise_OSError(errno);
			return mp_const_none;
		}
		res = rename(old_path, new_path);
	}
	*/
	if (res < 0) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(native_vfs_rename_obj, native_vfs_rename);

STATIC mp_obj_t native_vfs_mkdir(mp_obj_t vfs_in, mp_obj_t path_o) {
//	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_o);

	path = mkabspath(path);
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	ESP_LOGD(TAG, "mkdir('%s')", path);
	int res = mkdir(path, 0755);
	if (res < 0) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_mkdir_obj, native_vfs_mkdir);

/// Change current directory.
STATIC mp_obj_t native_vfs_chdir(mp_obj_t vfs_in, mp_obj_t path_in) {
//	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	path = mkabspath(path);
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	ESP_LOGD(TAG, "chdir('%s')", path);
	int res = chdir(path);
	if (res < 0) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_chdir_obj, native_vfs_chdir);

/// Get the current directory.
STATIC mp_obj_t native_vfs_getcwd(mp_obj_t vfs_in) {
//	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);

	char buf[MICROPY_ALLOC_PATH_MAX + 1];
	ESP_LOGD(TAG, "getcwd()");
	char *ch = getcwd(buf, sizeof(buf));
	if (ch == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	return mp_obj_new_str(buf, strlen(buf), false);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(native_vfs_getcwd_obj, native_vfs_getcwd);

/// \function stat(path)
/// Get the status of a file or directory.
STATIC mp_obj_t native_vfs_stat(mp_obj_t vfs_in, mp_obj_t path_in) {
//	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	path = mkabspath(path);
	if (path == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	ESP_LOGD(TAG, "stat('%s')", path);
	struct stat buf;
	if (path[0] == 0 || (path[0] == '/' && path[1] == 0)) {
		// stat root directory
		buf.st_size = 0;
		buf.st_atime = 946684800; // Jan 1, 2000
		buf.st_mode = MP_S_IFDIR;
	} else {
		int res = stat(path, &buf);
		if (res < 0) {
			mp_raise_OSError(errno);
			return mp_const_none;
		}
	}

	mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(10, NULL));
	t->items[0] = MP_OBJ_NEW_SMALL_INT(buf.st_mode);
	t->items[1] = MP_OBJ_NEW_SMALL_INT(0); // st_ino
	t->items[2] = MP_OBJ_NEW_SMALL_INT(0); // st_dev
	t->items[3] = MP_OBJ_NEW_SMALL_INT(0); // st_nlink
	t->items[4] = MP_OBJ_NEW_SMALL_INT(0); // st_uid
	t->items[5] = MP_OBJ_NEW_SMALL_INT(0); // st_gid
	t->items[6] = MP_OBJ_NEW_SMALL_INT(buf.st_size); // st_size
	t->items[7] = MP_OBJ_NEW_SMALL_INT(buf.st_atime); // st_atime
	t->items[8] = MP_OBJ_NEW_SMALL_INT(buf.st_atime); // st_mtime
	t->items[9] = MP_OBJ_NEW_SMALL_INT(buf.st_atime); // st_ctime

	return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_stat_obj, native_vfs_stat);

// Get the status of a VFS.
STATIC mp_obj_t native_vfs_statvfs(mp_obj_t vfs_in, mp_obj_t path_in) {
//	mp_obj_native_vfs_t *self = MP_OBJ_TO_PTR(vfs_in);
	const char *path = mp_obj_str_get_str(path_in);

	ESP_LOGD(TAG, "statvfs('%s')", path);

	// not supported
	mp_raise_OSError(ENOENT);
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(native_vfs_statvfs_obj, native_vfs_statvfs);

STATIC mp_obj_t native_vfs_mount(mp_obj_t self_in, mp_obj_t readonly, mp_obj_t mkfs) {
//	fs_user_mount_t *self = MP_OBJ_TO_PTR(self_in);

	bool flag_ro   = mp_obj_is_true(readonly);
	bool flag_mkfs = mp_obj_is_true(mkfs);

	ESP_LOGD(TAG, "mount(%s, %s)",
			flag_ro   ? "true" : "false",
			flag_mkfs ? "true" : "false");

	/* we do not support mount, but will do an initial mount on first call */

	// already mounted?
	if (native_vfs_mounted)
	{
		return mp_const_none;
	}

	// mount the block device
	const esp_vfs_fat_mount_config_t mount_config = {
		.max_files              = 8,
		.format_if_mount_failed = true,
	};

	ESP_LOGI(TAG, "mounting locfd on /");
	esp_err_t err = esp_vfs_fat_spiflash_mount("", "locfd", &mount_config, &s_wl_handle);

	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Failed to mount FATFS (0x%x)", err);
		mp_raise_OSError(MP_EIO);
	}

	native_vfs_mounted = true;

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(native_vfs_mount_obj, native_vfs_mount);

STATIC mp_obj_t native_vfs_umount(mp_obj_t self_in) {
//	fs_user_mount_t *self = MP_OBJ_TO_PTR(self_in);

	ESP_LOGD(TAG, "umount()");

	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(native_vfs_umount_obj, native_vfs_umount);

STATIC const mp_rom_map_elem_t native_vfs_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR_mkfs), MP_ROM_PTR(&native_vfs_mkfs_obj) },
	{ MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&native_vfs_open_obj) },
	{ MP_ROM_QSTR(MP_QSTR_ilistdir), MP_ROM_PTR(&native_vfs_ilistdir_obj) },
	{ MP_ROM_QSTR(MP_QSTR_mkdir), MP_ROM_PTR(&native_vfs_mkdir_obj) },
	{ MP_ROM_QSTR(MP_QSTR_rmdir), MP_ROM_PTR(&native_vfs_rmdir_obj) },
	{ MP_ROM_QSTR(MP_QSTR_chdir), MP_ROM_PTR(&native_vfs_chdir_obj) },
	{ MP_ROM_QSTR(MP_QSTR_getcwd), MP_ROM_PTR(&native_vfs_getcwd_obj) },
	{ MP_ROM_QSTR(MP_QSTR_remove), MP_ROM_PTR(&native_vfs_remove_obj) },
	{ MP_ROM_QSTR(MP_QSTR_rename), MP_ROM_PTR(&native_vfs_rename_obj) },
	{ MP_ROM_QSTR(MP_QSTR_stat), MP_ROM_PTR(&native_vfs_stat_obj) },
	{ MP_ROM_QSTR(MP_QSTR_statvfs), MP_ROM_PTR(&native_vfs_statvfs_obj) },
	{ MP_ROM_QSTR(MP_QSTR_mount), MP_ROM_PTR(&native_vfs_mount_obj) },
	{ MP_ROM_QSTR(MP_QSTR_umount), MP_ROM_PTR(&native_vfs_umount_obj) },
};
STATIC MP_DEFINE_CONST_DICT(native_vfs_locals_dict, native_vfs_locals_dict_table);

const mp_obj_type_t mp_native_vfs_type = {
	{ &mp_type_type },
	.name = MP_QSTR_VfsNative,
	.make_new = native_vfs_make_new,
	.locals_dict = (mp_obj_dict_t*)&native_vfs_locals_dict,
};

#endif // MICROPY_VFS_NATIVE
