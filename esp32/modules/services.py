import uos

# SHA2017 badge services
#   Renze Nicolai
#   Thomas Roos

def setup():
    global services
    try:
        apps = uos.listdir('lib')
    except OSError:
        print("[SERVICES] Can't setup services: no lib folder!")
        return False
    status = True #True if no error occured
    for app in apps:
        try:
            files = uos.listdir('lib/'+app)
        except OSError:
            print("[SERVICES] Listing app files for app '"+app+"' failed!")
            return False

        found = False
        for f in files:
            if (f=="service.py"):
                found = True
                print("[SERVICES] Running service "+app+"...")
                try:
                    srv = __import__('lib/'+app+'/service')
                    services.append(srv) #Add to global list
                    srv.setup()
                except BaseException as msg:
                    print("Exception in service setup "+app+": ", msg)
                    status = False #Error: status is now False
                break
        if not found:
            print("[SERVICES] App '"+app+"' has no service")
    return status

def loop(lcnt):
    noSleep = False
    global services
    for srv in services:
        try:
            if (srv.loop(lcnt)):
                noSleep = True
        except BaseException as msg:
            print("[SERVICES] Service loop exception: ", msg)
    return noSleep

def draw():
    noSleep = False
    global services
    x = 0
    y = 114
    for srv in services:
        try:
            space_used = srv.draw(x,y)
            if (space_used>0):
                y = y - abs(space_used)
        except BaseException as msg:
            print("[SERVICES] Service draw exception: ", msg)
