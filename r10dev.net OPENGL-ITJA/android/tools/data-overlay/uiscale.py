# Corner UI-size / text-size control for the intro screens (login / empire /
# character select / create). The in-game options panel only exists in the
# game phase, so a bad ui_scale could leave these screens unusable; this
# widget fixes it from the screens themselves. Apply takes effect live: the
# engine resizes the logical canvas and regenerates font atlases, then the
# current screen is rebuilt behind the phase curtain — no restart.
import weakref
import ui
import wndMgr
import app
import uimobilehud

BTN_SMALL = "d:/ymir work/ui/public/small_button_%02d.sub"
BTN_LARGE = "d:/ymir work/ui/public/large_button_%02d.sub"


def _MakeButton(parent, art, text, x, y, event):
	btn = ui.Button()
	btn.SetParent(parent)
	btn.SetUpVisual(art % 1)
	btn.SetOverVisual(art % 2)
	btn.SetDownVisual(art % 3)
	btn.SetPosition(x, y)
	btn.SetText(text)
	btn.SetEvent(event)
	btn.Show()
	return btn


class _ScaleRow(object):
	STEP = 0.05

	def __init__(self, parent, width, y, caption, value, lo, hi, onchange):
		self.lo = lo
		self.hi = hi
		self.value = value
		self.caption = caption
		self.onchange = onchange

		self.minusBtn = _MakeButton(parent, BTN_SMALL, "-", 8, y,
		                            ui.__mem_func__(self.OnStepDown))
		self.plusBtn = _MakeButton(parent, BTN_SMALL, "+",
		                           width - 8 - self.minusBtn.GetWidth(), y,
		                           ui.__mem_func__(self.OnStepUp))

		self.label = ui.TextLine()
		self.label.SetParent(parent)
		self.label.SetPosition(width / 2, y + self.minusBtn.GetHeight() / 2)
		self.label.SetHorizontalAlignCenter()
		self.label.SetVerticalAlignCenter()
		self.label.Show()
		self.__UpdateLabel()

	def OnStepDown(self):
		self.__Step(-self.STEP)

	def OnStepUp(self):
		self.__Step(self.STEP)

	def __Step(self, delta):
		self.value = uimobilehud._Clamp(round((self.value + delta) * 20) / 20.0,
		                                self.lo, self.hi)
		self.__UpdateLabel()
		self.onchange()

	def __UpdateLabel(self):
		self.label.SetText("%s %d%%" % (self.caption, int(round(self.value * 100))))


class IntroScaleWindow(ui.Window):
	W = 200
	H = 92
	MARGIN = 8

	def __init__(self, owner):
		ui.Window.__init__(self)
		self.SetWindowName("IntroScaleWindow")
		self.owner = weakref.proxy(owner)
		self.SetSize(self.W, self.H)

		# LoadDisplayConfig already parses and clamps the scale keys to floats.
		self.conf = uimobilehud.LoadDisplayConfig()

		self.board = ui.Bar()
		self.board.SetParent(self)
		self.board.SetSize(self.W, self.H)
		self.board.SetColor(0xC0000000)
		self.board.Show()

		self.uiRow = _ScaleRow(self, self.W, 6, "UI size",
		                       float(self.conf["ui_scale"]),
		                       uimobilehud.UI_SCALE_MIN, uimobilehud.UI_SCALE_MAX,
		                       self.OnRowChanged)
		self.textRow = _ScaleRow(self, self.W, 34, "Text size",
		                         float(self.conf["font_scale"]),
		                         uimobilehud.FONT_SCALE_MIN, uimobilehud.FONT_SCALE_MAX,
		                         self.OnRowChanged)

		self.applyBtn = _MakeButton(self, BTN_LARGE, "Apply", 0, 64,
		                            ui.__mem_func__(self.OnApply))
		self.applyBtn.SetPosition((self.W - self.applyBtn.GetWidth()) / 2, 64)

	def __del__(self):
		ui.Window.__del__(self)

	def __IsDirty(self):
		return (abs(self.uiRow.value - float(self.conf["ui_scale"])) > 0.001 or
		        abs(self.textRow.value - float(self.conf["font_scale"])) > 0.001)

	def OnRowChanged(self):
		pass

	def OnApply(self):
		if not self.__IsDirty():
			return
		self.conf["ui_scale"] = self.uiRow.value
		self.conf["font_scale"] = self.textRow.value
		uimobilehud.SaveDisplayConfig(self.conf)
		# Rebuild from the curtain's fade-out callback, not from inside this
		# button's own click handler: rebuilding destroys this widget.
		owner = self.owner
		stream = owner.stream
		stream.curtain.FadeOut(lambda: _Rebuild(owner, stream))


def _Rebuild(owner, stream):
	app.ApplyUIScale()
	owner.Close()
	owner.stream = stream  # CreateCharacterWindow.Close clears it
	owner.Open()
	stream.curtain.FadeIn()


_current = None


def Relayout():
	"""Rebuilds the open intro screen for a new surface size. False when none is open."""
	hud = _current
	try:
		if not hud or not hud.IsShow():
			return False
		owner = hud.owner
		stream = owner.stream
		if not stream:
			return False
	except Exception:  # owner window already destroyed
		return False
	stream.curtain.FadeOut(lambda: _Rebuild(owner, stream))
	return True


def Attach(window):
	global _current
	hud = IntroScaleWindow(window)
	_current = hud
	hud.SetParent(window)
	hud.SetPosition(wndMgr.GetScreenWidth() - hud.W - hud.MARGIN,
	                wndMgr.GetScreenHeight() - hud.H - hud.MARGIN)
	hud.SetTop()
	hud.Show()
	return hud
