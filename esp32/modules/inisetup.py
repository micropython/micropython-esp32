import uos

def setup():
    print("Performing initial setup")
    vfs = uos.VfsNative(True)
    import create
    create.boot()
    return vfs
