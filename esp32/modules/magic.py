import ugfx, appglue, hashlib, ubinascii, time, uos

names = ["Niek Blankers", "Sebastian Oort", "Bas van Sisseren", "Jeroen Domburg", "Christel Sanders", "Markus Bechtold", "Thomas Roos", "Anne Jan Brouwer", "Renze Nicolai", "Aram Verstegen", "Arnout Engelen", "Alexandre Dulaunoy", " Eric Poulsen", "Damien P. George", "uGFX", "EMF Badge Team"]

def action_exit(pushed):
    if (pushed):
        appglue.home()

def fill_screen_with_crap(c):
    print("Filling screen...")
    for i in range(0,3):
        nr = int.from_bytes(uos.urandom(1), 1)
        sha = hashlib.sha1(str(nr))
        s = ubinascii.hexlify(sha.digest())
        for j in range(0,8):
            x = 0 #int.from_bytes(uos.urandom(1), 1)
            y = int.from_bytes(uos.urandom(1), 1)
            if (x<ugfx.width()) and (y<ugfx.height()):
                ugfx.string(x, y, s+s+s, "Roboto_Regular12", c)
        ugfx.flush()
    print("done.")

def draw_name(x,y,name):
    print("Drawing name "+name)
    for i in range(1,5):
        ugfx.string(x-i, y-i, name, "PermanentMarker22", ugfx.WHITE)
        ugfx.string(x+i, y+i, name, "PermanentMarker22", ugfx.WHITE)
        ugfx.string(x-i, y+i, name, "PermanentMarker22", ugfx.WHITE)
        ugfx.string(x+i, y-i, name, "PermanentMarker22", ugfx.WHITE)
        ugfx.string(x-i, y, name, "PermanentMarker22", ugfx.WHITE)
        ugfx.string(x+i, y, name, "PermanentMarker22", ugfx.WHITE)
        ugfx.string(x, y-i, name, "PermanentMarker22", ugfx.WHITE)
        ugfx.string(x, y+i, name, "PermanentMarker22", ugfx.WHITE)
    ugfx.string(x, y, name, "PermanentMarker22", ugfx.BLACK)
    print("done.")
    ugfx.flush()

def show_names():
    global names
    c = False
    for n in range(0, len(names)):
        color = ugfx.BLACK
        if (c):
            c = False
        else:
            c = True
            color = ugfx.WHITE
        fill_screen_with_crap(color)
        x = int.from_bytes(uos.urandom(1), 1)
        y = round(int.from_bytes(uos.urandom(1), 1)/2)
        w = ugfx.get_string_width(names[n],"PermanentMarker22")
        if (x+w > ugfx.width()):
            x = x+w
        if (y > ugfx.height()-22):
            y = ugfx.height()-22
        if (x==0):
            x=1
        if (y==0):
            y=1
        print("NAME AT "+str(x)+", "+str(y))
        draw_name(x,y,names[n])
        time.sleep(1)

def main():
    #ugfx.init()
    ugfx.input_init()
    ugfx.input_attach(ugfx.BTN_B, action_exit)
    ugfx.input_attach(ugfx.BTN_START, action_exit)
    fill_screen_with_crap(ugfx.BLACK)
    show_names()

while (True):
    main()
