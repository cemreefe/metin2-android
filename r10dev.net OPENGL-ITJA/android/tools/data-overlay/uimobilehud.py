# Touch HUD for the Android build: an attack button ringed by big quick-slot buttons, pick-up,
# and a wheel menu for what desktop players reach through hotkeys. Loaded only when m2profile
# exists (Android). The "HUD" game option switches between it and the desktop taskbar.
import math
import app, item, net, player, skill, wndMgr
import emotion, mouseModule, ui

ART = "mobile/"
CONFIG = "mobilehud.cfg"
SLOT = 58
ATTACK = 84
SMALL = 50
REFRESH_SEC = 0.25
LABEL_COLOR = 0xffe6d6b4
T = "d:/ymir work/ui/game/taskbar/"
# stock taskbar icons carry their own square frame; crop it inside the round buttons
FRAMED = 0.13

# taskbar children that the mobile HUD replaces; the HP/SP/EXP gauges stay
DESKTOP_ONLY = ("Base_Board_01", "LeftMouseButton", "RightMouseButton", "CharacterButton",
		"InventoryButton", "MessengerButton", "SystemButton", "quickslot_board")

_hud = None


def IsMobileMode():
	try:
		f = open(CONFIG, "r")
		try:
			return f.read().strip() != "desktop"
		finally:
			f.close()
	except IOError:
		return True


def SetMobileMode(mobile):
	try:
		f = open(CONFIG, "w")
		try:
			f.write("mobile" if mobile else "desktop")
		finally:
			f.close()
	except IOError:
		pass
	if _hud:
		_hud.ApplyMode()


class _Label(ui.Window):
	def __init__(self, parent, height):
		ui.Window.__init__(self)
		self.SetParent(parent)
		self.AddFlag("not_pick")
		self.height = height
		self.back = ui.ExpandedImageBox()
		self.back.SetParent(self)
		self.back.AddFlag("not_pick")
		self.back.LoadImage(ART + "label.tga")
		self.back.Show()
		self.text = ui.TextLine()
		self.text.SetParent(self)
		self.text.AddFlag("not_pick")
		self.text.SetPackedFontColor(LABEL_COLOR)
		self.text.SetHorizontalAlignCenter()
		self.text.SetVerticalAlignCenter()
		self.text.Show()

	def SetText(self, text, centerX, top):
		self.text.SetText(text)
		(tw, th) = self.text.GetTextSize()
		w = max(self.height + 4, tw + 12)
		self.SetSize(w, self.height)
		self.back.SetScale(float(w) / self.back.GetWidth(), float(self.height) / self.back.GetHeight())
		self.text.SetPosition(w / 2, self.height / 2)
		self.SetPosition(centerX - w / 2, top)
		self.Show()


class _Disc(ui.Window):
	def __init__(self, size, onTap, onPress = None):
		ui.Window.__init__(self)
		self.SetSize(size, size)
		self.size = size
		self.onTap = onTap
		self.onPress = onPress
		self.base = self.__Image(ART + "button.tga", 1.0)
		self.icon = ui.ExpandedImageBox()
		self.icon.SetParent(self)
		self.icon.AddFlag("not_pick")
		self.cool = self.__Image(ART + "cooldown.tga", 1.0)
		self.cool.Hide()
		self.glow = self.__Image(ART + "active.tga", 1.0)
		self.glow.Hide()
		self.label = None
		self.iconName = None
		self.Show()

	def __Image(self, name, frac):
		img = ui.ExpandedImageBox()
		img.SetParent(self)
		img.AddFlag("not_pick")
		img.LoadImage(name)
		side = int(self.size * frac)
		img.SetScale(float(side) / img.GetWidth(), float(side) / img.GetHeight())
		img.SetPosition((self.size - side) / 2, (self.size - side) / 2)
		img.Show()
		return img

	def SetIcon(self, name, frac = 0.56, crop = 0.0):
		if name == self.iconName:
			return
		self.iconName = name
		if not name:
			self.icon.Hide()
			return
		self.icon.LoadImage(name)
		w, h = self.icon.GetWidth(), self.icon.GetHeight()
		if w <= 0 or h <= 0:
			self.icon.Hide()
			return
		side = self.size * frac
		s = min(side / (w * (1.0 - 2 * crop)), side / (h * (1.0 - 2 * crop)))
		self.icon.SetScale(s, s)
		self.icon.SetRenderingRect(-crop, -crop, -crop, -crop)
		self.icon.SetPosition(int((self.size - w * s) / 2), int((self.size - h * s) / 2))
		self.icon.Show()

	# key hint sits on the lower rim; names hang below the button
	def SetLabel(self, text, below = False):
		if not self.label:
			self.label = _Label(self, 18 if below else 15)
		self.label.SetText(text, self.size / 2, self.size + 2 if below else self.size - 15)

	def SetCoolDown(self, fraction):
		if fraction <= 0.0:
			self.cool.Hide()
			return
		self.cool.SetRenderingRect(0.0, -(1.0 - fraction), 0.0, 0.0)
		self.cool.Show()

	def SetActive(self, on):
		if on:
			self.glow.Show()
		else:
			self.glow.Hide()

	def __Pressed(self, down):
		alpha = 0.7 if down else 1.0
		self.base.SetAlpha(alpha)
		self.icon.SetAlpha(alpha)

	def OnMouseLeftButtonDown(self):
		self.__Pressed(True)
		if self.onPress:
			self.onPress(True)
		return True

	def OnMouseLeftButtonUp(self):
		self.__Pressed(False)
		if self.onPress:
			self.onPress(False)
		elif self.IsIn():
			self.onTap()
		return True


class _Wheel(ui.Window):
	RADIUS = 175
	ITEM = 64

	def __init__(self, entries):
		ui.Window.__init__(self, "TOP_MOST")
		w, h = wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()
		self.SetSize(w, h)
		self.dim = ui.Bar("TOP_MOST")
		self.dim.SetParent(self)
		self.dim.AddFlag("not_pick")
		self.dim.SetSize(w, h)
		self.dim.SetColor(0x70000000)
		self.dim.Show()
		self.discs = []
		cx, cy = w / 2, h / 2 - 10
		for i, (label, icon, crop, action) in enumerate(entries):
			a = -math.pi / 2 + 2 * math.pi * i / len(entries)
			d = _Disc(self.ITEM, self.__Choose(action))
			d.SetParent(self)
			d.SetIcon(icon, 0.5, crop)
			d.SetLabel(label, True)
			d.SetPosition(int(cx + self.RADIUS * math.cos(a)) - self.ITEM / 2,
					int(cy + self.RADIUS * 0.78 * math.sin(a)) - self.ITEM / 2)
			self.discs.append(d)
		self.Hide()

	def __Choose(self, action):
		def run():
			self.Hide()
			action()
		return run

	def Toggle(self):
		if self.IsShow():
			self.Hide()
		else:
			self.Show()
			self.SetTop()

	def OnMouseLeftButtonUp(self):
		self.Hide()
		return True


class HudOption:
	"""The 'HUD: Desktop / Mobile' row appended to the game options dialog."""
	ROOT = "d:/ymir work/ui/public/"

	def __init__(self, board, y, labelX, dataX, buttonWidth):
		self.label = ui.TextLine()
		self.label.SetParent(board)
		self.label.SetPosition(labelX, y + 2)
		self.label.SetText("HUD")
		self.label.Show()
		self.buttons = []
		for n, (text, mobile) in enumerate((("Desktop", False), ("Mobile", True))):
			b = ui.RadioButton()
			b.SetParent(board)
			b.SetUpVisual(self.ROOT + "middle_button_01.sub")
			b.SetOverVisual(self.ROOT + "middle_button_02.sub")
			b.SetDownVisual(self.ROOT + "middle_button_03.sub")
			b.SetPosition(dataX + buttonWidth * n, y)
			b.SetText(text)
			b.SetEvent(self.__Select, mobile)
			b.Show()
			self.buttons.append(b)
		self.__Refresh()

	def __Select(self, mobile):
		SetMobileMode(mobile)
		self.__Refresh()

	def __Refresh(self):
		on = 1 if IsMobileMode() else 0
		for n, b in enumerate(self.buttons):
			if n == on:
				b.Down()
			else:
				b.SetUp()

	def Destroy(self):
		self.buttons = []
		self.label = None


DISPLAY_CONFIG = "display.cfg"
UI_SCALE_MIN = 1.0
UI_SCALE_MAX = 1.2
CAMERA_MIN = 0.25
CAMERA_MAX = 2.0


def _Clamp(value, low, high):
	return max(low, min(high, value))


def LoadDisplayConfig():
	"""display.cfg holds 'key value' lines; the engine reads ui_scale and camera_sensitivity."""
	conf = {"ui_scale": "1.0", "orientation": "landscape", "camera_sensitivity": "1.0"}
	try:
		f = open(DISPLAY_CONFIG, "r")
		try:
			for line in f.readlines():
				parts = line.split()
				if len(parts) == 2:
					conf[parts[0]] = parts[1]
		finally:
			f.close()
	except IOError:
		pass
	for key, low, high in (("ui_scale", UI_SCALE_MIN, UI_SCALE_MAX), ("camera_sensitivity", CAMERA_MIN, CAMERA_MAX)):
		try:
			conf[key] = _Clamp(float(conf[key]), low, high)
		except ValueError:
			conf[key] = 1.0
	conf["portrait"] = conf["orientation"] == "portrait"
	return conf


def SaveDisplayConfig(conf):
	try:
		f = open(DISPLAY_CONFIG, "w")
		try:
			f.write("ui_scale %.2f\norientation %s\ncamera_sensitivity %.2f\n" % (
				_Clamp(conf["ui_scale"], UI_SCALE_MIN, UI_SCALE_MAX),
				"portrait" if conf["portrait"] else "landscape",
				_Clamp(conf["camera_sensitivity"], CAMERA_MIN, CAMERA_MAX)))
		finally:
			f.close()
	except IOError:
		pass


class _NextFrame(ui.Window):
	"""Runs a callback on the next UI update, outside the event handler that requested it."""

	def __init__(self, func):
		ui.Window.__init__(self)
		self.func = func
		self.Show()

	def OnUpdate(self):
		func, self.func = self.func, None
		self.Hide()
		global _pending
		_pending = None
		if func:
			func()


_pending = None


def _RunNextFrame(func):
	global _pending
	_pending = _NextFrame(func)


def _RebuildGameUI(game, reopenOptions):
	"""Resizes the logical canvas and recreates the game UI in place; the connection,
	world and player state stay as they are and the windows refill from them."""
	import gc
	import interfaceModule, uiTarget
	old = game.interface
	quests = [(btn.index, btn.name) for btn in old.questButtonList]
	if game.mobileHud:
		game.mobileHud.Destroy()
		game.mobileHud = None
	mouseModule.mouseController.DeattachObject()
	old.HideAllWindows()
	old.Close()
	game.interface = None
	old = None
	gc.collect()

	app.ApplyUIScale()
	w, h = wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()
	game.SetSize(w, h)

	interface = interfaceModule.Interface()
	interface.MakeInterface()
	interface.ShowDefaultWindows()
	game.interface = interface
	for index, name in reversed(quests):
		interface.RecvQuest(index, name)

	if game.targetBoard:
		game.targetBoard.Hide()
		game.targetBoard.Destroy()
	game.targetBoard = uiTarget.TargetBoard()
	game.targetBoard.SetWhisperEvent(ui.__mem_func__(interface.OpenWhisperDialog))
	game.targetBoard.Hide()
	game.console.SetConsoleSize(w, 200)
	game.mobileHud = MobileHud(game)

	game.StartGame()
	game.RefreshStatus()
	game.RefreshQuest()
	if reopenOptions:
		interface.ToggleSystemDialog()
		interface.dlgSystem._SystemDialog__ClickGameOptionButton()


def ApplyUIScaleLive():
	"""True when the current phase can be rescaled in place (it is rebuilt next frame)."""
	if not _hud or not _hud.game or not _hud.game.interface:
		return False
	game = _hud.game

	def rebuild():
		try:
			_RebuildGameUI(game, True)
		except Exception:
			import dbg, sys
			dbg.TraceError("live UI rescale failed: %s; restarting" % str(sys.exc_info()[1]))
			app.RestartApplication()
	_RunNextFrame(rebuild)
	return True


class DisplayOption:
	"""Game option rows: UI size (applied in place), camera sensitivity (applied on the next
	drag) and orientation (needs a restart, the activity orientation is fixed at launch)."""
	ROOT = "d:/ymir work/ui/public/"
	ROWS = 4

	def __init__(self, board, y, labelX, dataX, buttonWidth):
		self.conf = LoadDisplayConfig()
		self.appliedScale = self.conf["ui_scale"]
		self.runningPortrait = self.conf["portrait"]
		self.children = []

		self.sizeLabel = self.__Text(board, labelX, y + 2, "")
		self.sizeSlider = self.__Slider(board, dataX, y + 2,
			(self.conf["ui_scale"] - UI_SCALE_MIN) / (UI_SCALE_MAX - UI_SCALE_MIN), self.__OnSlideSize)

		self.cameraLabel = self.__Text(board, labelX, y + 27, "")
		self.cameraSlider = self.__Slider(board, dataX, y + 27,
			(self.conf["camera_sensitivity"] - CAMERA_MIN) / (CAMERA_MAX - CAMERA_MIN), self.__OnSlideCamera)

		self.__Text(board, labelX, y + 52, "Screen")
		self.orientButtons = []
		for n, (text, portrait) in enumerate((("Landscape", False), ("Portrait", True))):
			b = self.__Button(board, dataX + buttonWidth * n, y + 50, text, ui.RadioButton())
			b.SetEvent(self.__SelectOrientation, portrait)
			self.orientButtons.append(b)

		self.apply = self.__Button(board, dataX, y + 75, "Apply", ui.Button())
		self.apply.SetEvent(ui.__mem_func__(self.__Apply))
		self.__Refresh()

	def __Text(self, board, x, y, text):
		t = ui.TextLine()
		t.SetParent(board)
		t.SetPosition(x, y)
		t.SetText(text)
		t.Show()
		self.children.append(t)
		return t

	def __Button(self, board, x, y, text, b):
		b.SetParent(board)
		b.SetUpVisual(self.ROOT + "middle_button_01.sub")
		b.SetOverVisual(self.ROOT + "middle_button_02.sub")
		b.SetDownVisual(self.ROOT + "middle_button_03.sub")
		b.SetPosition(x, y)
		b.SetText(text)
		b.Show()
		self.children.append(b)
		return b

	def __Slider(self, board, x, y, pos, event):
		slider = ui.SliderBar()
		slider.SetParent(board)
		slider.SetPosition(x, y)
		slider.SetSliderPos(_Clamp(pos, 0.0, 1.0))
		slider.SetEvent(ui.__mem_func__(event))
		slider.Show()
		self.children.append(slider)
		return slider

	def __OnSlideSize(self):
		pos = _Clamp(self.sizeSlider.GetSliderPos(), 0.0, 1.0)
		self.conf["ui_scale"] = round(UI_SCALE_MIN + pos * (UI_SCALE_MAX - UI_SCALE_MIN), 2)
		self.__Refresh()

	def __OnSlideCamera(self):
		pos = _Clamp(self.cameraSlider.GetSliderPos(), 0.0, 1.0)
		self.conf["camera_sensitivity"] = round(CAMERA_MIN + pos * (CAMERA_MAX - CAMERA_MIN), 2)
		SaveDisplayConfig(self.conf)
		self.__Refresh()

	def __SelectOrientation(self, portrait):
		self.conf["portrait"] = portrait
		self.__Refresh()

	def __Apply(self):
		SaveDisplayConfig(self.conf)
		if self.conf["portrait"] != self.runningPortrait:
			app.RestartApplication()
			return
		if ApplyUIScaleLive():
			self.appliedScale = self.conf["ui_scale"]
			self.apply.Hide()
		else:
			app.RestartApplication()

	def __Refresh(self):
		self.sizeLabel.SetText("UI size %d%%" % int(self.conf["ui_scale"] * 100 + 0.5))
		self.cameraLabel.SetText("Camera %d%%" % int(self.conf["camera_sensitivity"] * 100 + 0.5))
		on = 1 if self.conf["portrait"] else 0
		for n, b in enumerate(self.orientButtons):
			if n == on:
				b.Down()
			else:
				b.SetUp()
		if self.conf["portrait"] != self.runningPortrait:
			self.apply.SetText("Restart to apply")
			self.apply.Show()
		elif abs(self.conf["ui_scale"] - self.appliedScale) > 0.001:
			self.apply.SetText("Apply")
			self.apply.Show()
		else:
			self.apply.Hide()

	def Destroy(self):
		self.children = []
		self.orientButtons = []
		self.sizeSlider = None
		self.cameraSlider = None
		self.apply = None
		self.sizeLabel = None
		self.cameraLabel = None


class MobileHud(ui.Window):
	# quick slot index -> keyboard key it mirrors
	SLOTS = ((0, "1"), (1, "2"), (2, "3"), (3, "4"), (4, "F1"), (5, "F2"), (6, "F3"), (7, "F4"))

	def __init__(self, game):
		ui.Window.__init__(self)
		global _hud
		self.game = game
		w, h = wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()
		self.SetSize(w, h)
		self.AddFlag("not_pick")

		cx, cy = w - 78, h - 82
		self.attack = self.__Disc(ATTACK, None, cx - ATTACK / 2, cy - ATTACK / 2, self.__Attack)
		self.attack.SetIcon(T + "mouse_button_attack_01.sub", 0.6, FRAMED)

		self.slots = []
		for n, (index, key) in enumerate(self.SLOTS):
			ring, step = (0, n) if n < 4 else (1, n - 4)
			r = 106 if ring == 0 else 172
			a = math.radians(180 + 90 * (step + 0.5 * ring) / (3.0 + 0.5 * ring))
			x = int(cx + r * math.cos(a)) - SLOT / 2
			y = int(cy + r * math.sin(a)) - SLOT / 2
			d = self.__Disc(SLOT, self.__UseSlot(index), x, y)
			d.SetLabel(key)
			self.slots.append(d)

		top = cy - 172 - SLOT / 2 - 14
		self.pickup = self.__Disc(SMALL, player.PickCloseItem, w - SMALL - 10, top - SMALL)
		self.pickup.SetIcon(T + "mouse_button_move_01.sub", 0.56, FRAMED)
		self.menuButton = self.__Disc(SMALL, self.__ToggleWheel, w - SMALL - 10, top - 2 * SMALL - 12)
		self.menuButton.SetIcon(T + "system_button_01.sub", 0.56, FRAMED)

		self.wheel = _Wheel(self.__WheelEntries())
		self.nextRefresh = 0.0
		self.taskBarWidth = None
		_hud = self
		self.ApplyMode()

	def __Disc(self, size, onTap, x, y, onPress = None):
		d = _Disc(size, onTap, onPress)
		d.SetParent(self)
		d.SetPosition(x, y)
		return d

	def __Attack(self, down):
		player.SetAttackKeyState(down)

	def __UseSlot(self, index):
		if app.IsRTL():
			index = 3 - index if index < 4 else 11 - index
		def use():
			if mouseModule.mouseController.isAttached():
				self.game.interface.wndTaskBar.AddQuickSlot(index)
			else:
				player.RequestUseLocalQuickSlot(index)
		return use

	def __ToggleWheel(self):
		self.wheel.Toggle()

	def __ToggleNames(self):
		if self.game.ShowNameFlag:
			self.game.HideName()
		else:
			self.game.ShowName()

	def __WheelEntries(self):
		i = self.game.interface
		return (
			("Bag", T + "inventory_button_01.sub", FRAMED, i.ToggleInventoryWindow),
			("Character", T + "character_button_01.sub", FRAMED, i.ToggleCharacterWindowStatusPage),
			("Skills", T + "mouse_button_skill_01.sub", FRAMED, lambda: i.ToggleCharacterWindow("SKILL")),
			("Quests", "d:/ymir work/ui/game/quest/questicon/level_05.sub", 0.0, lambda: i.ToggleCharacterWindow("QUEST")),
			("Emotes", "d:/ymir work/ui/game/windows/emotion_clap.sub", 0.0, lambda: i.ToggleCharacterWindow("EMOTICON")),
			("Map", T + "m.sub", FRAMED, i.PressMKey),
			("Chat log", T + "open_chat_log_button_01.sub", FRAMED, i.ToggleChatLogWindow),
			("Friends", T + "community_button_01.sub", FRAMED, i.ToggleMessenger),
			("Names", T + "g.sub", FRAMED, self.__ToggleNames),
			("Ride", T + "mouse_button_move_and_attack_01.sub", FRAMED, lambda: net.SendChatPacket("/ride")),
			("Settings", T + "system_button_01.sub", FRAMED, i.ToggleSystemDialog),
		)

	def ApplyMode(self):
		mobile = IsMobileMode()
		taskBar = self.game.interface.wndTaskBar
		if taskBar:
			if self.taskBarWidth is None:
				self.taskBarWidth = taskBar.GetWidth()
			for name in DESKTOP_ONLY:
				child = taskBar.GetChild(name)
				if mobile:
					child.Hide()
				else:
					child.Show()
			# the taskbar root would otherwise swallow touches along the whole bottom edge
			exp = taskBar.GetChild("EXP_Gauge_Board")
			(ex, ey) = exp.GetLocalPosition()
			width = ex + exp.GetWidth() if mobile else self.taskBarWidth
			taskBar.SetSize(width, taskBar.GetHeight())
			if mobile and self.game.interface.wndExpandedTaskBar:
				self.game.interface.wndExpandedTaskBar.Hide()
		player.SetAttackKeyState(False)
		if mobile:
			self.Show()
		else:
			self.wheel.Hide()
			self.Hide()

	def __SlotIcon(self, index):
		(kind, pos) = player.GetLocalQuickSlot(index)
		if kind == player.SLOT_TYPE_INVENTORY:
			vnum = player.GetItemIndex(pos)
			if not vnum:
				return None, 0.0, False
			item.SelectItem(vnum)
			return item.GetIconImageFileName(), 0.0, False
		if kind == player.SLOT_TYPE_SKILL:
			skillIndex = player.GetSkillIndex(pos)
			if not skillIndex:
				return None, 0.0, False
			cool = 0.0
			if player.IsSkillCoolTime(pos):
				(total, elapsed) = player.GetSkillCoolTime(pos)
				if total > 0:
					cool = max(0.0, 1.0 - elapsed / total)
			return skill.GetIconName(skillIndex, player.GetSkillGrade(pos)), cool, player.IsSkillActive(pos)
		if kind == player.SLOT_TYPE_EMOTION:
			return emotion.ICON_DICT.get(pos), 0.0, False
		return None, 0.0, False

	def OnUpdate(self):
		now = app.GetTime()
		if now < self.nextRefresh:
			return
		self.nextRefresh = now + REFRESH_SEC
		for d, (index, key) in zip(self.slots, self.SLOTS):
			icon, cool, active = self.__SlotIcon(index)
			d.SetIcon(icon)
			d.SetCoolDown(cool)
			d.SetActive(active)

	def Destroy(self):
		global _hud
		if _hud is self:
			_hud = None
		player.SetAttackKeyState(False)
		self.wheel.Hide()
		self.wheel = None
		self.slots = []
		self.Hide()
