import sys
try:
    import uerrno
    try:
        import uos_vfs as uos
        open = uos.vfs_open
    except ImportError:
        import uos
except ImportError:
    print("SKIP")
    sys.exit()

try:
    uos.VfsFat
except AttributeError:
    print("SKIP")
    sys.exit()


class RAMFS:

    SEC_SIZE = 512

    def __init__(self, blocks):
        self.data = bytearray(blocks * self.SEC_SIZE)

    def readblocks(self, n, buf):
        #print("readblocks(%s, %x(%d))" % (n, id(buf), len(buf)))
        for i in range(len(buf)):
            buf[i] = self.data[n * self.SEC_SIZE + i]

    def writeblocks(self, n, buf):
        #print("writeblocks(%s, %x)" % (n, id(buf)))
        for i in range(len(buf)):
            self.data[n * self.SEC_SIZE + i] = buf[i]

    def ioctl(self, op, arg):
        #print("ioctl(%d, %r)" % (op, arg))
        if op == 4:  # BP_IOCTL_SEC_COUNT
            return len(self.data) // self.SEC_SIZE
        if op == 5:  # BP_IOCTL_SEC_SIZE
            return self.SEC_SIZE


try:
    bdev = RAMFS(50)
except MemoryError:
    print("SKIP")
    sys.exit()

uos.VfsFat.mkfs(bdev)
vfs = uos.VfsFat(bdev)
uos.mount(vfs, '/ramdisk')
uos.chdir('/ramdisk')

try:
    vfs.mkdir("foo_dir")
except OSError as e:
    print(e.args[0] == uerrno.EEXIST)

try:
    vfs.remove("foo_dir")
except OSError as e:
    print(e.args[0] == uerrno.EISDIR)

try:
    vfs.remove("no_file.txt")
except OSError as e:
    print(e.args[0] == uerrno.ENOENT)

try:
    vfs.rename("foo_dir", "/null/file")
except OSError as e:
    print(e.args[0] == uerrno.ENOENT)

# file in dir
with open("foo_dir/file-in-dir.txt", "w+t") as f:
    f.write("data in file")

with open("foo_dir/file-in-dir.txt", "r+b") as f:
    print(f.read())

with open("foo_dir/sub_file.txt", "w") as f:
    f.write("subdir file")

# directory not empty
try:
    vfs.rmdir("foo_dir")
except OSError as e:
    print(e.args[0] == uerrno.EACCES)

# trim full path
vfs.rename("foo_dir/file-in-dir.txt", "foo_dir/file.txt")
print(vfs.listdir("foo_dir"))

vfs.rename("foo_dir/file.txt", "moved-to-root.txt")
print(vfs.listdir())

# check that renaming to existing file will overwrite it
with open("temp", "w") as f:
    f.write("new text")
vfs.rename("temp", "moved-to-root.txt")
print(vfs.listdir())
with open("moved-to-root.txt") as f:
    print(f.read())

# valid removes
vfs.remove("foo_dir/sub_file.txt")
vfs.rmdir("foo_dir")
print(vfs.listdir())

# disk full
try:
    bsize = vfs.statvfs("/ramdisk")[0]
    free = vfs.statvfs("/ramdisk")[2] + 1
    f = open("large_file.txt", "wb")
    f.write(bytearray(bsize * free))
except OSError as e:
    print("ENOSPC:", e.args[0] == 28) # uerrno.ENOSPC
