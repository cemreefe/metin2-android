# Hidden developer options, toggled by typing "devops$" anywhere in the game
# (the engine watches key input and calls Toggle).
import ui
import wndMgr
import app
import uimobilehud

FONTS = (
	("Roboto", "default"),
	("DejaVu Sans", "fonts/dejavusans.ttf"),
	("DejaVu Serif", "fonts/dejavuserif.ttf"),
	("Liberation Sans", "fonts/liberationsans.ttf"),
	("Liberation Serif", "fonts/liberationserif.ttf"),
)
BTN = "d:/ymir work/ui/public/xlarge_button_%02d.sub"


def _MakeButton(parent, text, x, y, event):
	btn = ui.Button()
	btn.SetParent(parent)
	btn.SetUpVisual(BTN % 1)
	btn.SetOverVisual(BTN % 2)
	btn.SetDownVisual(BTN % 3)
	btn.SetPosition(x, y)
	btn.SetText(text)
	btn.SetEvent(event)
	btn.Show()
	return btn


class DevOptionsWindow(ui.Window):
	W = 220
	PAD = 10
	ROW = 30

	def __init__(self):
		ui.Window.__init__(self)
		self.SetWindowName("DevOptionsWindow")
		self.H = self.PAD * 2 + 22 + self.ROW * (len(FONTS) + 1)
		self.SetSize(self.W, self.H)

		self.board = ui.Bar()
		self.board.SetParent(self)
		self.board.SetSize(self.W, self.H)
		self.board.SetColor(0xE0000000)
		self.board.Show()

		self.title = ui.TextLine()
		self.title.SetParent(self)
		self.title.SetPosition(self.W / 2, self.PAD)
		self.title.SetHorizontalAlignCenter()
		self.title.SetText("Dev options: font")
		self.title.Show()

		self.fontButtons = []
		y = self.PAD + 22
		for name, path in FONTS:
			btn = _MakeButton(self, name, 0, y, lambda p=path: self.OnPickFont(p))
			btn.SetPosition((self.W - btn.GetWidth()) / 2, y)
			self.fontButtons.append((btn, name, path))
			y += self.ROW
		self.closeBtn = _MakeButton(self, "Close", 0, y, ui.__mem_func__(self.Hide))
		self.closeBtn.SetPosition((self.W - self.closeBtn.GetWidth()) / 2, y)

	def __del__(self):
		ui.Window.__del__(self)

	def Refresh(self):
		current = uimobilehud.LoadDisplayConfig().get("font", "default")
		for btn, name, path in self.fontButtons:
			btn.SetText(("> %s <" % name) if path == current else name)

	def OnPickFont(self, path):
		if not app.SetPortFont("" if path == "default" else path, 1):
			return
		conf = uimobilehud.LoadDisplayConfig()
		conf["font"] = path
		uimobilehud.SaveDisplayConfig(conf)
		self.Refresh()

	def OnPressEscapeKey(self):
		self.Hide()
		return True


_window = None


def Toggle():
	global _window
	if _window and _window.IsShow():
		_window.Hide()
		return
	if not _window:
		_window = DevOptionsWindow()
	_window.SetPosition((wndMgr.GetScreenWidth() - _window.W) / 2,
	                    (wndMgr.GetScreenHeight() - _window.H) / 2)
	_window.Refresh()
	_window.Show()
	_window.SetTop()
