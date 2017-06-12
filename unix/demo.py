import ugfx

ugfx.init()

ugfx.clear(badge.BLACK)

ugfx.fill_circle(60, 60, 50, badge.WHITE);
ugfx.fill_circle(60, 60, 40, badge.BLACK);
ugfx.fill_circle(60, 60, 30, badge.WHITE);
ugfx.fill_circle(60, 60, 20, badge.BLACK);
ugfx.fill_circle(60, 60, 10, badge.WHITE);

ugfx.thickline(1,1,100,100,badge.WHITE,10,5)
ugfx.box(30,30,50,50,badge.WHITE)

ugfx.string(150,25,"STILL","Roboto_BlackItalic24",badge.WHITE)
ugfx.string(130,50,"Hacking","PermanentMarker22",badge.WHITE)
len = ugfx.get_string_width("Hacking","PermanentMarker22")
ugfx.line(130, 72, 144 + len, 72, badge.WHITE)
ugfx.line(140 + len, 52, 140 + len, 70, badge.WHITE)
ugfx.string(140,75,"Anyway","Roboto_BlackItalic24",badge.WHITE)

ugfx.flush()
