# Touch HUD for the Android build: big quick-slot buttons, pick-up, and a wheel menu for
# what desktop players reach through hotkeys. Loaded only when m2profile exists (Android).
import math
import app, item, net, player, skill, wndMgr
import emotion, ui

ART = "mobile/"
SLOT = 58
PICKUP = 70
REFRESH_SEC = 0.25


class _Disc(ui.Window):
	def __init__(self, size, onTap):
		ui.Window.__init__(self)
		self.SetSize(size, size)
		self.size = size
		self.onTap = onTap
		self.base = self.__Image(ART + "button.tga", 1.0)
		self.icon = ui.ExpandedImageBox()
		self.icon.SetParent(self)
		self.icon.AddFlag("not_pick")
		self.cool = self.__Image(ART + "cooldown.tga", 0.84)
		self.cool.Hide()
		self.glow = self.__Image(ART + "active.tga", 1.0)
		self.glow.Hide()
		self.label = ui.TextLine()
		self.label.SetParent(self)
		self.label.AddFlag("not_pick")
		self.label.SetHorizontalAlignCenter()
		self.label.SetPosition(size / 2, size - 14)
		self.label.SetOutline()
		self.label.Show()
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

	def SetIcon(self, name, frac = 0.62):
		if name == self.iconName:
			return
		self.iconName = name
		if not name:
			self.icon.Hide()
			return
		self.icon.LoadImage(name)
		side = int(self.size * frac)
		w, h = self.icon.GetWidth(), self.icon.GetHeight()
		if w <= 0 or h <= 0:
			self.icon.Hide()
			return
		s = min(float(side) / w, float(side) / h)
		self.icon.SetScale(s, s)
		self.icon.SetPosition((self.size - int(w * s)) / 2, (self.size - int(h * s)) / 2)
		self.icon.Show()

	def SetLabel(self, text):
		self.label.SetText(text)

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

	def OnMouseLeftButtonDown(self):
		self.base.SetAlpha(0.6)
		return True

	def OnMouseLeftButtonUp(self):
		self.base.SetAlpha(1.0)
		if self.IsIn():
			self.onTap()
		return True


class _Wheel(ui.Window):
	RADIUS = 150
	ITEM = 66

	def __init__(self, entries):
		ui.Window.__init__(self, "TOP_MOST")
		self.SetSize(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
		self.dim = ui.Bar("TOP_MOST")
		self.dim.SetParent(self)
		self.dim.AddFlag("not_pick")
		self.dim.SetSize(wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight())
		self.dim.SetColor(0x88000000)
		self.dim.Show()
		self.discs = []
		cx, cy = wndMgr.GetScreenWidth() / 2, wndMgr.GetScreenHeight() / 2
		for i, (label, icon, action) in enumerate(entries):
			a = -math.pi / 2 + 2 * math.pi * i / len(entries)
			d = _Disc(self.ITEM, self.__Choose(action))
			d.SetParent(self)
			d.SetIcon(icon)
			d.SetLabel(label)
			d.SetPosition(int(cx + self.RADIUS * math.cos(a)) - self.ITEM / 2,
					int(cy + self.RADIUS * 0.82 * math.sin(a)) - self.ITEM / 2)
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


class MobileHud(ui.Window):
	# quick slot index -> keyboard key it mirrors
	SLOTS = ((0, "1"), (1, "2"), (2, "3"), (3, "4"), (4, "F1"), (5, "F2"), (6, "F3"), (7, "F4"))

	def __init__(self, game):
		ui.Window.__init__(self)
		self.game = game
		w, h = wndMgr.GetScreenWidth(), wndMgr.GetScreenHeight()
		self.SetSize(w, h)
		self.AddFlag("not_pick")
		self.Show()

		cx, cy = w - 64, h - 112
		self.pickup = self.__Disc(PICKUP, player.PickCloseItem, cx - PICKUP / 2, cy - PICKUP / 2)
		self.pickup.SetIcon("d:/ymir work/ui/game/taskbar/mouse_button_move_01.sub")
		self.pickup.SetLabel("Pick up")

		self.slots = []
		for n, (index, key) in enumerate(self.SLOTS):
			ring, step = (0, n) if n < 4 else (1, n - 4)
			r = 118 if ring == 0 else 190
			a = math.radians(180 + 90 * step / 3.0)
			x = int(cx + r * math.cos(a)) - SLOT / 2
			y = int(cy + r * math.sin(a)) - SLOT / 2
			d = self.__Disc(SLOT, self.__UseSlot(index), x, y)
			d.SetLabel(key)
			self.slots.append(d)

		self.menuButton = self.__Disc(SLOT, self.__ToggleWheel, w - SLOT - 8, h - 112 - 190 - SLOT - 36)
		self.menuButton.SetIcon("d:/ymir work/ui/game/taskbar/system_button_01.sub")
		self.menuButton.SetLabel("Menu")

		self.wheel = _Wheel(self.__WheelEntries())
		self.nextRefresh = 0.0

	def __Disc(self, size, onTap, x, y):
		d = _Disc(size, onTap)
		d.SetParent(self)
		d.SetPosition(x, y)
		return d

	def __UseSlot(self, index):
		if app.IsRTL():
			index = 3 - index if index < 4 else 11 - index
		return lambda: player.RequestUseLocalQuickSlot(index)

	def __ToggleWheel(self):
		self.wheel.Toggle()

	def __ToggleNames(self):
		if self.game.ShowNameFlag:
			self.game.HideName()
		else:
			self.game.ShowName()

	def __WheelEntries(self):
		i = self.game.interface
		T = "d:/ymir work/ui/game/taskbar/"
		return (
			("Bag", T + "inventory_button_01.sub", i.ToggleInventoryWindow),
			("Character", T + "character_button_01.sub", i.ToggleCharacterWindowStatusPage),
			("Skills", T + "mouse_button_skill_01.sub", lambda: i.ToggleCharacterWindow("SKILL")),
			("Quests", "d:/ymir work/ui/game/quest/questicon/level_05.sub", lambda: i.ToggleCharacterWindow("QUEST")),
			("Emotes", "d:/ymir work/ui/game/windows/emotion_clap.sub", lambda: i.ToggleCharacterWindow("EMOTICON")),
			("Map", T + "m.sub", i.PressMKey),
			("Chat log", T + "open_chat_log_button_01.sub", i.ToggleChatLogWindow),
			("Friends", T + "community_button_01.sub", i.ToggleMessenger),
			("Names", T + "g.sub", self.__ToggleNames),
			("Ride", T + "mouse_button_move_and_attack_01.sub", lambda: net.SendChatPacket("/ride")),
			("Settings", T + "system_button_01.sub", i.ToggleSystemDialog),
		)

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
		self.wheel.Hide()
		self.wheel = None
		self.slots = []
		self.Hide()
