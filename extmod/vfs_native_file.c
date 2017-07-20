#include "py/mpconfig.h"
#if MICROPY_VFS && MICROPY_VFS_NATIVE

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "py/nlr.h"
#include "py/runtime.h"
#include "py/stream.h"
#include "py/mperrno.h"
#include "extmod/vfs_native.h"

#include "src/ff.h"

#include "esp_vfs.h"
#include "esp_system.h"
#include "esp_log.h"

static const char *TAG = "vfs_native_file.c";

extern const mp_obj_type_t mp_type_fileio;
extern const mp_obj_type_t mp_type_textio;

typedef struct _pyb_file_obj_t {
	mp_obj_base_t base;
	int fd;
} pyb_file_obj_t;

STATIC void file_obj_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
	pyb_file_obj_t *self = MP_OBJ_TO_PTR(self_in);
	(void)kind;

	mp_printf(print, "<io.%s %d>", mp_obj_get_type_str(self_in), self->fd);
}

STATIC mp_uint_t file_obj_read(mp_obj_t self_in, void *buf, mp_uint_t size, int *errcode) {
	pyb_file_obj_t *self = MP_OBJ_TO_PTR(self_in);

	ESP_LOGV(TAG, "read(%d, buf, %d)", self->fd, size);
	int sz_out = read(self->fd, buf, size);
	ESP_LOGV(TAG, " `-> %d", sz_out);
	if (sz_out < 0) {
		ESP_LOGE(TAG, "read(%d, buf, %d): error %d", self->fd, size, errno);
		*errcode = errno;
		return MP_STREAM_ERROR;
	}
	return sz_out;
}

STATIC mp_uint_t file_obj_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
	pyb_file_obj_t *self = MP_OBJ_TO_PTR(self_in);

	ESP_LOGV(TAG, "write(%d, buf, %d)", self->fd, size);
	int sz_out = write(self->fd, buf, size);
	ESP_LOGV(TAG, " `-> %d", sz_out);
	if (sz_out < 0) {
		ESP_LOGE(TAG, "write(%d, buf, %d): error %d", self->fd, size, errno);
		*errcode = errno;
		return MP_STREAM_ERROR;
	}
	mp_uint_t sz_out_sum = sz_out;
	while (sz_out > 0) {
		buf = &((const uint8_t *) buf)[sz_out];
		size -= sz_out;
		ESP_LOGV(TAG, "write(%d, buf, %d)", self->fd, size);
		sz_out = write(self->fd, buf, size);
		ESP_LOGV(TAG, " `-> %d", sz_out);
		if (sz_out < 0) {
			ESP_LOGE(TAG, "write(%d, buf, %d): error %d", self->fd, size, errno);
			*errcode = errno;
			return MP_STREAM_ERROR;
		}
		sz_out_sum += sz_out;
	}

	return sz_out_sum;
}


STATIC mp_obj_t file_obj_close(mp_obj_t self_in) {
	pyb_file_obj_t *self = MP_OBJ_TO_PTR(self_in);
	// if fs==NULL then the file is closed and in that case this method is a no-op
	if (self->fd != -1) {
		ESP_LOGD(TAG, "close()");
		int res = close(self->fd);
		self->fd = -1;
		if (res < 0) {
			ESP_LOGE(TAG, "close(%d): error %d", self->fd, errno);
			mp_raise_OSError(errno);
		}
	}
	return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(file_obj_close_obj, file_obj_close);

STATIC mp_obj_t file_obj___exit__(size_t n_args, const mp_obj_t *args) {
	(void)n_args;
	return file_obj_close(args[0]);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(file_obj___exit___obj, 4, 4, file_obj___exit__);

STATIC mp_uint_t file_obj_ioctl(mp_obj_t o_in, mp_uint_t request, uintptr_t arg, int *errcode) {
	pyb_file_obj_t *self = MP_OBJ_TO_PTR(o_in);

	ESP_LOGD(TAG, "ioctl(%u)", request);
	if (request == MP_STREAM_SEEK) {
		struct mp_stream_seek_t *s = (struct mp_stream_seek_t*)(uintptr_t)arg;

		off_t off = lseek(self->fd, s->offset, s->whence);
		if (off == (off_t)-1) {
			ESP_LOGE(TAG, "ioctl(%d, %d, ..): error %d", self->fd, request, errno);
			*errcode = errno;
			return MP_STREAM_ERROR;
		}
		s->offset = off;
		return 0;

	} else if (request == MP_STREAM_FLUSH) {
		// fsync() not implemented.
		return 0;

	} else {
		ESP_LOGE(TAG, "ioctl(%d, %d, ..): error %d", self->fd, request, MP_EINVAL);
		*errcode = MP_EINVAL;
		return MP_STREAM_ERROR;
	}
}

// Note: encoding is ignored for now; it's also not a valid kwarg for CPython's FileIO,
// but by adding it here we can use one single mp_arg_t array for open() and FileIO's constructor
STATIC const mp_arg_t file_open_args[] = {
	{ MP_QSTR_file, MP_ARG_OBJ | MP_ARG_REQUIRED, {.u_rom_obj = MP_ROM_PTR(&mp_const_none_obj)} },
	{ MP_QSTR_mode, MP_ARG_OBJ, {.u_obj = MP_OBJ_NEW_QSTR(MP_QSTR_r)} },
	{ MP_QSTR_encoding, MP_ARG_OBJ | MP_ARG_KW_ONLY, {.u_rom_obj = MP_ROM_PTR(&mp_const_none_obj)} },
};
#define FILE_OPEN_NUM_ARGS MP_ARRAY_SIZE(file_open_args)

STATIC mp_obj_t file_open(fs_user_mount_t *vfs, const mp_obj_type_t *type, mp_arg_val_t *args) {
	pyb_file_obj_t *o = m_new_obj_with_finaliser(pyb_file_obj_t);
	o->base.type = type;

	const char *fname = mp_obj_str_get_str(args[0].u_obj);
	const char *mode_s = mp_obj_str_get_str(args[1].u_obj);

	fname = mkabspath(fname);
	if (fname == NULL) {
		mp_raise_OSError(errno);
		return mp_const_none;
	}

	const char *mode_s_orig = mode_s;
	ESP_LOGD(TAG, "open('%s', '%s')", fname, mode_s_orig);

	int mode_rw = 0, mode_x = 0;
	while (*mode_s) {
		switch (*mode_s++) {
			case 'r':
				mode_rw = O_RDONLY;
				break;
			case 'w':
				mode_rw = O_WRONLY;
				mode_x = O_CREAT | O_TRUNC;
				break;
			case 'a':
				mode_rw = O_WRONLY;
				mode_x = O_CREAT | O_APPEND;
				break;
			case '+':
				mode_rw = O_RDWR;
				break;
#if MICROPY_PY_IO_FILEIO
				// If we don't have io.FileIO, then files are in text mode implicitly
			case 'b':
				type = &mp_type_fileio;
				break;
			case 't':
				type = &mp_type_textio;
				break;
#endif
		}
	}

	assert(vfs != NULL);
	int fd = open(fname, mode_x | mode_rw, 0644);
	if (fd == -1) {
		ESP_LOGE(TAG, "open('%s', '%s'): error %d", fname, mode_s_orig, errno);
		m_del_obj(pyb_file_obj_t, o);
		mp_raise_OSError(errno);
	}
	o->fd = fd;

	return MP_OBJ_FROM_PTR(o);
}

STATIC mp_obj_t file_obj_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
	mp_arg_val_t arg_vals[FILE_OPEN_NUM_ARGS];
	mp_arg_parse_all_kw_array(n_args, n_kw, args, FILE_OPEN_NUM_ARGS, file_open_args, arg_vals);
	return file_open(NULL, type, arg_vals);
}

// TODO gc hook to close the file if not already closed

STATIC const mp_rom_map_elem_t rawfile_locals_dict_table[] = {
	{ MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
	{ MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
	{ MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
	{ MP_ROM_QSTR(MP_QSTR_readlines), MP_ROM_PTR(&mp_stream_unbuffered_readlines_obj) },
	{ MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
	{ MP_ROM_QSTR(MP_QSTR_flush), MP_ROM_PTR(&mp_stream_flush_obj) },
	{ MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&file_obj_close_obj) },
	{ MP_ROM_QSTR(MP_QSTR_seek), MP_ROM_PTR(&mp_stream_seek_obj) },
	{ MP_ROM_QSTR(MP_QSTR_tell), MP_ROM_PTR(&mp_stream_tell_obj) },
	{ MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&file_obj_close_obj) },
	{ MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj) },
	{ MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&file_obj___exit___obj) },
};

STATIC MP_DEFINE_CONST_DICT(rawfile_locals_dict, rawfile_locals_dict_table);

#if MICROPY_PY_IO_FILEIO
STATIC const mp_stream_p_t fileio_stream_p = {
	.read = file_obj_read,
	.write = file_obj_write,
	.ioctl = file_obj_ioctl,
};

const mp_obj_type_t mp_type_fileio = {
	{ &mp_type_type },
	.name = MP_QSTR_FileIO,
	.print = file_obj_print,
	.make_new = file_obj_make_new,
	.getiter = mp_identity_getiter,
	.iternext = mp_stream_unbuffered_iter,
	.protocol = &fileio_stream_p,
	.locals_dict = (mp_obj_dict_t*)&rawfile_locals_dict,
};
#endif

STATIC const mp_stream_p_t textio_stream_p = {
	.read = file_obj_read,
	.write = file_obj_write,
	.ioctl = file_obj_ioctl,
	.is_text = true,
};

const mp_obj_type_t mp_type_textio = {
	{ &mp_type_type },
	.name = MP_QSTR_TextIOWrapper,
	.print = file_obj_print,
	.make_new = file_obj_make_new,
	.getiter = mp_identity_getiter,
	.iternext = mp_stream_unbuffered_iter,
	.protocol = &textio_stream_p,
	.locals_dict = (mp_obj_dict_t*)&rawfile_locals_dict,
};

// Factory function for I/O stream classes
mp_obj_t nativefs_builtin_open_self(mp_obj_t self_in, mp_obj_t path, mp_obj_t mode) {
	// TODO: analyze buffering args and instantiate appropriate type
	fs_user_mount_t *self = MP_OBJ_TO_PTR(self_in);
	mp_arg_val_t arg_vals[FILE_OPEN_NUM_ARGS];
	arg_vals[0].u_obj = path;
	arg_vals[1].u_obj = mode;
	arg_vals[2].u_obj = mp_const_none;
	return file_open(self, &mp_type_textio, arg_vals);
}

#endif // MICROPY_VFS && MICROPY_VFS_NATIVE
