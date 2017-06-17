import badge
import ugfx

badge.init()
ugfx.init()

ugfx.clear(ugfx.BLACK)

ugfx.fill_circle(60, 60, 50, ugfx.WHITE);
ugfx.fill_circle(60, 60, 40, ugfx.BLACK);
ugfx.fill_circle(60, 60, 30, ugfx.WHITE);
ugfx.fill_circle(60, 60, 20, ugfx.BLACK);
ugfx.fill_circle(60, 60, 10, ugfx.WHITE);

ugfx.thickline(1,1,100,100,ugfx.WHITE,10,5)
ugfx.box(30,30,50,50,ugfx.WHITE)

ugfx.string(150,25,"STILL","Roboto_BlackItalic24",ugfx.WHITE)
ugfx.string(130,50,"Hacking","PermanentMarker22",ugfx.WHITE)
len = ugfx.get_string_width("Hacking","PermanentMarker22")
ugfx.line(130, 72, 144 + len, 72, ugfx.WHITE)
ugfx.line(140 + len, 52, 140 + len, 70, ugfx.WHITE)
ugfx.string(140,75,"Anyway","Roboto_BlackItalic24",ugfx.WHITE)

ugfx.flush()

def render(text, pushed):
    if(pushed):
        ugfx.string(100,10,text,"PermanentMarker22",ugfx.WHITE)
    else:
        ugfx.string(100,10,text,"PermanentMarker22",ugfx.BLACK)
    ugfx.flush()

ugfx.input_init()
ugfx.input_attach(ugfx.JOY_UP, lambda pressed: render('UP', pressed))
ugfx.input_attach(ugfx.JOY_DOWN, lambda pressed: render('DOWN', pressed))
ugfx.input_attach(ugfx.JOY_LEFT, lambda pressed: render('LEFT', pressed))
ugfx.input_attach(ugfx.JOY_RIGHT, lambda pressed: render('RIGHT', pressed))
ugfx.input_attach(ugfx.BTN_A, lambda pressed: render('A', pressed))
ugfx.input_attach(ugfx.BTN_B, lambda pressed: render('B', pressed))
ugfx.input_attach(ugfx.BTN_START, lambda pressed: render('Start', pressed))
ugfx.input_attach(ugfx.BTN_SELECT, lambda pressed: render('Select', pressed))

while True:
    pass
