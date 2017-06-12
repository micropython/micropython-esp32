import ugfx

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
