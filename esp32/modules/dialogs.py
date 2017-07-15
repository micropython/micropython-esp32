### Author: EMF Badge team
### Author: SHA2017Badge team
### Description: Some basic UGFX powered dialogs
### License: MIT

import ugfx, utime as time

wait_for_interrupt = True
button_pushed = ''

def notice(text, title="SHA2017", close_text="Close", width = 296, height = 128, font="Roboto_Regular12"):
	"""Show a notice which can be closed with button A.

	The caller is responsible for flushing the display after the user has confirmed the notice.
	"""
	prompt_boolean(text, title = title, true_text = close_text, false_text = None, width = width, height = height, font=font)

def prompt_boolean(text, title="SHA2017", true_text="Yes", false_text="No", width = 296, height = 128, font="Roboto_Regular12"):
	"""A simple one and two-options dialog

	if 'false_text' is set to None only one button is displayed.
	If both 'false_text' and 'true_text' are given a boolean is returned, press B for false, A for true.

	The caller is responsible for flushing the display after processing the response.
	"""
	global wait_for_interrupt, button_pushed

	window = ugfx.Container((ugfx.width() - width) // 2, (ugfx.height() - height) // 2,  width, height)
	window.show()
	ugfx.set_default_font(font)
	window.text(5, 10, title, ugfx.BLACK)
	window.line(0, 30, width, 30, ugfx.BLACK)

	if false_text:
		false_text = "B: " + false_text
		true_text = "A: " + true_text

	label = ugfx.Label(5, 30, width - 10, height - 80, text = text, parent=window)
	button_no = ugfx.Button(5, height - 40, width // 2 - 15, 30, false_text, parent=window) if false_text else None
	button_yes = ugfx.Button(width // 2 + 5 if true_text else 5, height - 40, width // 2 - 15 if false_text else width - 10, 30, true_text, parent=window)
	button_yes.set_focus()

	ugfx.input_init()

	window.show()
	ugfx.set_lut(ugfx.LUT_NORMAL)
	ugfx.flush()

	def done(value):
		window.hide()
		window.destroy()
		button_yes.destroy()
		if button_no: button_no.destroy()
		label.destroy()
		return value

	if button_no: ugfx.input_attach(ugfx.BTN_B, pressed_b)
	ugfx.input_attach(ugfx.BTN_A, pressed_a)

	wait_for_interrupt = True
	while wait_for_interrupt:
		time.sleep(0.2)

	if button_pushed == "B": return done(False)
	if button_pushed == "A": return done(True)

def prompt_text(description, init_text = "", true_text="OK", false_text="Back", width = 300, height = 200, font="Roboto_BlackItalic24"):
	"""Shows a dialog and keyboard that allows the user to input/change a string

	Returns None if user aborts with button B

	The caller is responsible for flushing the display after processing the response.
	"""
	global wait_for_interrupt, button_pushed

	window = ugfx.Container(int((ugfx.width()-width)/2), int((ugfx.height()-height)/2), width, height)
	window.show()

	ugfx.set_default_font("Roboto_Regular12")
	kb_height = int(height/2) + 30
	kb = ugfx.Keyboard(0, height - kb_height, width, kb_height, parent=window)

	ugfx.set_default_font("Roboto_Regular18")
	edit_height = 25
	edit = ugfx.Textbox(5, height-kb_height-5-edit_height, int(width*4/5)-10, edit_height, text = init_text, parent=window)
	ugfx.set_default_font("Roboto_Regular12")
	button_height = 25

	def okay(evt):
		# We'd like promises here, but for now this should do
		global wait_for_interrupt
		button_pushed = "A"
		wait_for_interrupt = False

	button_yes = ugfx.Button(int(width*4/5), height-kb_height-button_height, int(width*1/5)-3, button_height, true_text, parent=window, cb=okay)
	button_no = ugfx.Button(int(width*4/5), height-kb_height-button_height-button_height, int(width/5)-3, button_height, false_text, parent=window) if false_text else None
	ugfx.set_default_font(font)
	label = ugfx.Label(5, 1, int(width*4/5), height-kb_height-5-edit_height-5, description, parent=window)

	def vkey_backspace():
		edit.backspace()
		ugfx.flush()

	focus = 0

	def toggle_focus(pressed):
		if pressed:
			if focus == 0:
				edit.set_focus()
				kb.enabled(1)
				ugfx.input_attach(ugfx.BTN_B, lambda pressed: vkey_backspace() if pressed else 0)
				ugfx.input_attach(ugfx.BTN_A, lambda pressed: 0 if pressed else ugfx.flush())
				focus = 1
			elif focus == 1 or not button_no:
				button_yes.set_focus()
				kb.enabled(0)
				ugfx.input_attach(ugfx.BTN_A, pressed_a)
				ugfx.input_attach(ugfx.BTN_B, pressed_b)
				focus = (2 if button_no else 0)
			else:
				button_no.set_focus()
				kb.enabled(0)
				ugfx.input_attach(ugfx.BTN_A, pressed_a)
				ugfx.input_attach(ugfx.BTN_B, pressed_b)
				focus = 0
			ugfx.flush()

	ugfx.input_init()
	ugfx.input_attach(ugfx.BTN_SELECT, toggle_focus)
	ugfx.input_attach(ugfx.JOY_LEFT, lambda pressed: ugfx.flush() if pressed else 0)
	ugfx.input_attach(ugfx.JOY_RIGHT, lambda pressed: ugfx.flush() if pressed else 0)
	ugfx.input_attach(ugfx.JOY_UP, lambda pressed: ugfx.flush() if pressed else 0)
	ugfx.input_attach(ugfx.JOY_DOWN, lambda pressed: ugfx.flush() if pressed else 0)

	toggle_focus(True)

	ugfx.set_lut(ugfx.LUT_NORMAL)
	ugfx.flush()

	wait_for_interrupt = True
	while wait_for_interrupt:
		time.sleep(0.2)

	def done(value):
		window.hide()
		window.destroy()
		button_yes.destroy()
		if button_no: button_no.destroy()
		label.destroy()
		kb.destroy()
		edit.destroy()
		return value

	if (focus == 0 and no_button) or button_pushed == "B": return done(False)
	return done(edit.text())

def prompt_option(options, index=0, text = "Please select one of the following:", title=None, select_text="OK", none_text=None):
	"""Shows a dialog prompting for one of multiple options

	If none_text is specified the user can use the B or Menu button to skip the selection
	if title is specified a blue title will be displayed about the text
	"""
	global wait_for_interrupt, button_pushed

	ugfx.set_default_font("Roboto_Regular12")
	window = ugfx.Container(5, 5, ugfx.width() - 10, ugfx.height() - 10)
	window.show()

	list_y = 30
	if title:
		window.text(5, 10, title, ugfxBLACK)
		window.line(0, 25, ugfx.width() - 10, 25, ugfx.BLACK)
		window.text(5, 30, text, ugfx.BLACK)
		list_y = 50
	else:
		window.text(5, 10, text, ugfx.BLACK)

	options_list = ugfx.List(5, list_y, ugfx.width() - 25, 180 - list_y, parent = window)

	for option in options:
		if isinstance(option, dict) and option["title"]:
			options_list.add_item(option["title"])
		else:
			options_list.add_item(str(option))
	options_list.selected_index(index)

	select_text = "A: " + select_text
	if none_text:
		none_text = "B: " + none_text

	button_select = ugfx.Button(5, ugfx.height() - 50, 140 if none_text else ugfx.width() - 25, 30 , select_text, parent=window)
	button_none = ugfx.Button(ugfx.width() - 160, ugfx.height() - 50, 140, 30 , none_text, parent=window) if none_text else None

	try:
		ugfx.input_init()

		wait_for_interrupt = True
		while wait_for_interrupt:
			if button_pushed == "A": return options[options_list.selected_index()]
			if button_pushed == "B": return None
			if button_none and button_pushed == "START": return None
			time.sleep(0.2)

	finally:
		window.hide()
		window.destroy()
		options_list.destroy()
		button_select.destroy()
		if button_none: button_none.destroy()
		ugfx.poll()


def pressed_a(pushed):
	global wait_for_interrupt, button_pushed
	wait_for_interrupt = False
	button_pushed = 'A'

def pressed_b(pushed):
	global wait_for_interrupt, button_pushed
	wait_for_interrupt = False
	button_pushed = 'B'

def pressed_start(pushed):
	global wait_for_interrupt, button_pushed
	wait_for_interrupt = False
	button_pushed = 'START'

ugfx.input_attach(ugfx.BTN_A, pressed_a)
ugfx.input_attach(ugfx.BTN_B, pressed_b)
ugfx.input_attach(ugfx.BTN_START, pressed_start)

class WaitingMessage:
	"""Shows a dialog with a certain message that can not be dismissed by the user"""
	def __init__(self, text = "Please Wait...", title="SHA2017Badge"):
		self.window = ugfx.Container(30, 30, ugfx.width() - 60, ugfx.height() - 60)
		self.window.show()
		self.window.text(5, 10, title, ugfx.BLACK)
		self.window.line(0, 30, ugfx.width() - 60, 30, ugfx.BLACK)
		self.label = ugfx.Label(5, 40, self.window.width() - 10, ugfx.height() - 40, text = text, parent=self.window)

		# Indicator to show something is going on
		self.indicator = ugfx.Label(ugfx.width() - 100, 0, 20, 20, text = "...", parent=self.window)
		self.timer = pyb.Timer(3)
		self.timer.init(freq=3)
		self.timer.callback(lambda t: self.indicator.visible(not self.indicator.visible()))

	def destroy(self):
		self.timer.deinit()
		self.label.destroy()
		self.indicator.destroy()
		self.window.destroy()

	def text(self):
		return self.label.text()

	def text(self, value):
		self.label.text(value)

	def __enter__(self):
		return self

	def __exit__(self, exc_type, exc_value, traceback):
		self.destroy()
