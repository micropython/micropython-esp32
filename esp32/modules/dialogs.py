### Author: EMF Badge team
### Author: SHA2017Badge team
### Description: Some basic UGFX powered dialogs
### License: MIT

import ugfx, badge, utime as time

def notice(text, title="SHA2017", close_text="Close", width = 296, height = 128, font="Roboto_Regular12"):
	prompt_boolean(text, title = title, true_text = close_text, false_text = None, width = width, height = height, font=font)

def prompt_boolean(text, title="SHA2017", true_text="Yes", false_text="No", width = 296, height = 128, font="Roboto_Regular12"):
	"""A simple one and two-options dialog

	if 'false_text' is set to None only one button is displayed.
	If both 'true_text' and 'false_text' are given a boolean is returned
	"""
	window = ugfx.Container((ugfx.width() - width) // 2, (ugfx.height() - height) // 2,  width, height)
	window.show()
	ugfx.set_default_font(font)
	window.text(5, 10, title, TILDA_COLOR)
	window.line(0, 30, width, 30, ugfx.BLACK)

	if false_text:
		true_text = "A: " + true_text
		false_text = "B: " + false_text

	label = ugfx.Label(5, 30, width - 10, height - 80, text = text, parent=window)
	button_yes = ugfx.Button(5, height - 40, width // 2 - 15 if false_text else width - 15, 30 , true_text, parent=window)
	button_no = ugfx.Button(width // 2 + 5, height - 40, width // 2 - 15, 30 , false_text, parent=window) if false_text else None

	try:
		ugfx.input_init()

		# button_yes.attach_input(ugfx.BTN_A,0)
		# if button_no: button_no.attach_input(ugfx.BTN_B,0)

		window.show()

		# while True:
		# 	pyb.wfi() # TODO make this!!
		# 	if buttons.is_triggered("BTN_A"): return True
		# 	if buttons.is_triggered("BTN_B"): return False

	finally:
		window.hide()
		window.destroy()
		button_yes.destroy()
		if button_no: button_no.destroy()
		label.destroy()

def prompt_text(description, init_text = "", true_text="OK", false_text="Back", width = 300, height = 200, font="Roboto_BlackItalic24"):
	"""Shows a dialog and keyboard that allows the user to input/change a string

	Returns None if user aborts with button B
	"""

	window = ugfx.Container(int((ugfx.width()-width)/2), int((ugfx.height()-height)/2), width, height)

	if false_text:
		true_text = "M: " + true_text
		false_text = "B: " + false_text

	# if buttons.has_interrupt("BTN_MENU"):
	# 	buttons.disable_interrupt("BTN_MENU")

	ugfx.set_default_font("Roboto_Regular18")
	kb = ugfx.Keyboard(0, int(height/2), width, int(height/2), parent=window)
	edit = ugfx.Textbox(5, int(height/2)-30, int(width*4/5)-10, 25, text = init_text, parent=window)
	ugfx.set_default_font("Roboto_Regular12")
	button_yes = ugfx.Button(int(width*4/5), int(height/2)-30, int(width*1/5)-3, 25 , true_text, parent=window)
	button_no = ugfx.Button(int(width*4/5), int(height/2)-30-30, int(width/5)-3, 25 , false_text, parent=window) if false_text else None
	ugfx.set_default_font(font)
	label = ugfx.Label(int(width/10), int(height/10), int(width*4/5), int(height*2/5)-60, description, parent=window)

	try:
		ugfx.input_init()

		# button_yes.attach_input(ugfx.BTN_MENU,0)
		# if button_no: button_no.attach_input(ugfx.BTN_B,0)
		# TODO ^^

		window.show()
		edit.set_focus()
		# while True:
			# pyb.wfi()
			# ugfx.poll()
			# #if buttons.is_triggered("BTN_A"): return edit.text()
			# if buttons.is_triggered("BTN_B"): return None
			# if buttons.is_triggered("BTN_MENU"): return edit.text()

	finally:
		window.hide()
		window.destroy()
		button_yes.destroy()
		if button_no: button_no.destroy()
		label.destroy()
		kb.destroy()
		edit.destroy();
	return

def prompt_option(options, index=0, text = "Please select one of the following:", title=None, select_text="OK", none_text=None):
	"""Shows a dialog prompting for one of multiple options

	If none_text is specified the user can use the B or Menu button to skip the selection
	if title is specified a blue title will be displayed about the text
	"""
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

		# while True:
		# 	pyb.wfi()  # BLABLA TODO
		# 	ugfx.poll()
		# 	if buttons.is_triggered("BTN_A"): return options[options_list.selected_index()]
		# 	if button_none and buttons.is_triggered("BTN_B"): return None
		# 	if button_none and buttons.is_triggered("BTN_MENU"): return None

	finally:
		window.hide()
		window.destroy()
		options_list.destroy()
		button_select.destroy()
		if button_none: button_none.destroy()
		ugfx.poll()

class WaitingMessage:
	"""Shows a dialog with a certain message that can not be dismissed by the user"""
	def __init__(self, text = "Please Wait...", title="TiLDA"):
		self.window = ugfx.Container(30, 30, ugfx.width() - 60, ugfx.height() - 60)
		self.window.show()
		self.window.text(5, 10, title, TILDA_COLOR)
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
