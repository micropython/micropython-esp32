#include "py/lexer.h"
#include "py/obj.h"
#include "extmod/vfs.h"

// these are the values for fs_user_mount_t.flags
#define FSUSER_NATIVE       (0x0001) // readblocks[2]/writeblocks[2] contain native func
#define FSUSER_FREE_OBJ     (0x0002) // fs_user_mount_t obj should be freed on umount
#define FSUSER_HAVE_IOCTL   (0x0004) // new protocol with ioctl

typedef struct _fs_user_mount_t {
    mp_obj_base_t base;
    uint16_t flags;
    mp_obj_t readblocks[4];
    mp_obj_t writeblocks[4];
    // new protocol uses just ioctl, old uses sync (optional) and count
    union {
        mp_obj_t ioctl[4];
        struct {
            mp_obj_t sync[2];
            mp_obj_t count[2];
        } old;
    } u;
    //FATFS fatfs;
} fs_user_mount_t;

extern const byte fresult_to_errno_table[20];
extern const mp_obj_type_t mp_esp_vfs_type;

mp_import_stat_t esp_vfs_import_stat(struct _fs_user_mount_t *vfs, const char *path);
mp_obj_t fatfs_builtin_open_self(mp_obj_t self_in, mp_obj_t path, mp_obj_t mode);
MP_DECLARE_CONST_FUN_OBJ_KW(mp_builtin_open_obj);

mp_obj_t esp_vfs_ilistdir2(struct _fs_user_mount_t *vfs, const char *path, bool is_str_type);
