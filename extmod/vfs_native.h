#include "py/lexer.h"
#include "py/obj.h"
#include "extmod/vfs.h"

typedef struct _fs_user_mount_t {
    mp_obj_base_t base;
} fs_user_mount_t;

extern const mp_obj_type_t mp_native_vfs_type;

const char * mkabspath(const char *path, char *buf, int buflen);
mp_import_stat_t native_vfs_import_stat(struct _fs_user_mount_t *vfs, const char *path);
mp_obj_t nativefs_builtin_open_self(mp_obj_t self_in, mp_obj_t path, mp_obj_t mode);
MP_DECLARE_CONST_FUN_OBJ_KW(mp_builtin_open_obj);

mp_obj_t native_vfs_ilistdir2(struct _fs_user_mount_t *vfs, const char *path, bool is_str_type);
