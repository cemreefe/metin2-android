# Corner UI-size control for the intro screens (login / empire / character
# select). The in-game options panel only exists in the game phase, so a bad
# ui_scale can leave these screens unusable and unfixable from the UI; this
# widget lets the player recover from the screens themselves. A second row
# controls font_scale (text size) independently.
import ui
import wndMgr
import app
import uimobilehud

_POS = lambda v, lo, hi: (v - lo) / float(hi - lo)
_VAL = lambda p, lo, hi: lo + p * (hi - lo)


class _JumpArea(ui.Window):
	"""Overlay on the slider track: click or drag anywhere on the bar to set it.
	The stock cursor knob is ~10px, easy to miss — especially at high ui_scale."""

	def __init__(self, slider, onchange):
		ui.Window.__init__(self)
		self.slider = slider
		self.onchange = onchange

	def __Jump(self):
		mx, my = wndMgr.GetMousePosition()
		gx, gy = self.GetGlobalPosition()
		pos = uimobilehud._Clamp(float(mx - gx) / float(self.GetWidth()), 0.0, 1.0)
		self.slider.SetSliderPos(pos)
		self.onchange()

	def OnMouseLeftButtonDown(self):
		self.__Jump()

	def OnMouseDrag(self):
		self.__Jump()


class _ScaleRow(object):
	"""One row of the widget: '-' label '+' over a slider, driving a
	display.cfg scale value. Text and -/+ buttons both work — the -/+ pair
	steps the value, the slider track jumps it."""

	STEP = 0.05

	def __init__(self, parent, y, caption, value, lo, hi, onchange):
		self.parent = parent
		self.lo = lo
		self.hi = hi
		self.value = value
		self.onchange = onchange
		self.caption = caption

		# ui.Button's hit area is its own size, not the text's — a bare button
		# is 0x0 and can never be clicked, so every button gets a SetSize.
		self.minusBtn = ui.Button()
		self.minusBtn.SetParent(parent)
		self.minusBtn.SetText("-")
		self.minusBtn.SetSize(16, 14)
		self.minusBtn.SetPosition(6, y)
		self.minusBtn.SetEvent(ui.__mem_func__(self.OnStepDown))
		self.minusBtn.Show()

		self.label = ui.TextLine()
		self.label.SetParent(parent)
		self.label.SetPosition(28, y + 2)
		self.label.Show()

		self.plusBtn = ui.Button()
		self.plusBtn.SetParent(parent)
		self.plusBtn.SetText("+")
		self.plusBtn.SetSize(16, 14)
		self.plusBtn.SetPosition(118, y)
		self.plusBtn.SetEvent(ui.__mem_func__(self.OnStepUp))
		self.plusBtn.Show()

		self.slider = ui.SliderBar()
		self.slider.SetParent(parent)
		self.slider.SetPosition(8, y + 18)
		self.slider.SetEvent(ui.__mem_func__(self.OnSlide))
		self.slider.SetSliderPos(_POS(self.value, lo, hi))
		self.slider.Show()

		self.jump = _JumpArea(self.slider, self.OnSlide)
		self.jump.SetParent(parent)
		self.jump.SetPosition(8, y + 18)
		self.jump.SetSize(self.slider.GetWidth(), self.slider.GetHeight())
		self.jump.Show()

		self.__UpdateLabel()

	def OnStepDown(self):
		self.__Step(-self.STEP)

	def OnStepUp(self):
		self.__Step(self.STEP)

	def __Step(self, delta):
		self.value = uimobilehud._Clamp(round((self.value + delta) * 20) / 20,
		                                self.lo, self.hi)
		self.slider.SetSliderPos(_POS(self.value, self.lo, self.hi))
		self.__UpdateLabel()
		self.onchange()

	def OnSlide(self):
		self.value = _VAL(self.slider.GetSliderPos(), self.lo, self.hi)
		self.__UpdateLabel()
		self.onchange()

	def __UpdateLabel(self):
		self.label.SetText("%s %d%%" % (self.caption,
		                                int(round(self.value * 100))))


class IntroScaleWindow(ui.Window):
	W = 236
	H = 82

	def __init__(self):
		ui.Window.__init__(self)
		self.SetWindowName("IntroScaleWindow")

		self.SetSize(self.W, self.H)
		self.SetPosition(wndMgr.GetScreenWidth() - self.W - 6,
		                 wndMgr.GetScreenHeight() - self.H - 6)

		# LoadDisplayConfig already parses and clamps the scale keys to floats.
		self.conf = uimobilehud.LoadDisplayConfig()

		self.board = ui.Bar()
		self.board.SetSize(self.W, self.H)
		self.board.SetParent(self)
		self.board.SetPosition(0, 0)
		self.board.Show()

		self.uiRow = _ScaleRow(self, 4, "UI size",
		                       float(self.conf["ui_scale"]),
		                       uimobilehud.UI_SCALE_MIN,
		                       uimobilehud.UI_SCALE_MAX,
		                       self.OnRowChanged)
		self.textRow = _ScaleRow(self, 46, "text",
		                         float(self.conf["font_scale"]),
		                         uimobilehud.FONT_SCALE_MIN,
		                         uimobilehud.FONT_SCALE_MAX,
		                         self.OnRowChanged)

		self.applyBtn = ui.Button()
		self.applyBtn.SetParent(self)
		self.applyBtn.SetText("apply")
		self.applyBtn.SetSize(40, 14)
		self.applyBtn.SetPosition(self.W - 46, 4)
		self.applyBtn.SetEvent(ui.__mem_func__(self.OnApply))
		self.applyBtn.Show()

	def __del__(self):
		ui.Window.__del__(self)

	def OnRowChanged(self):
		pass

	def OnApply(self):
		self.conf["ui_scale"] = self.uiRow.value
		self.conf["font_scale"] = self.textRow.value
		uimobilehud.SaveDisplayConfig(self.conf)
		# The intro windows were laid out against the old logical size; the only
		# reliable way to re-lay them out is to relaunch. On the web build this
		# reloads the page (the passphrase survives in session storage); on
		# Android it restarts the activity.
		app.ApplyUIScale()
		app.RestartApplication()


def Attach(window):
	hud = IntroScaleWindow()
	hud.SetParent(window)
	hud.SetPosition(wndMgr.GetScreenWidth() - hud.W - 6,
	                wndMgr.GetScreenHeight() - hud.H - 6)
	hud.SetTop()
	hud.Show()
	return hud
