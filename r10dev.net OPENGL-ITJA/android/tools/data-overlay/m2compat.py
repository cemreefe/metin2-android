# Shims for script APIs that m2dev-client's root expects but this engine does not export.
import app, chat, chrmgr, item, miniMap, net, player, shop, systemSetting, wndMgr

def _noop(*args):
	return 0

def _define(module, name, value):
	if not hasattr(module, name):
		setattr(module, name, value)

_define(app, "GetLocalePathCommon", lambda: "locale/common")
_define(app, "IsRTL", lambda: 0)
_define(app, "ReloadLocale", _noop)
_define(app, "loggined", 0)
_define(chat, "SetAlign", _noop)
_define(chrmgr, "EFFECT_STATE", 900)
_define(chrmgr, "EFFECT_AGGREGATE_MONSTER", 901)
_define(item, "APPLY_PC_BANG_EXP_BONUS", 900)
_define(item, "APPLY_PC_BANG_DROP_BONUS", 901)
_define(item, "GetLimitType", lambda i: item.GetLimit(i)[0])
_define(item, "GetLimitValue", lambda i: item.GetLimit(i)[1])
_define(miniMap, "RegisterColor", _noop)
_define(net, "SendQuestCancelPacket", _noop)
_define(net, "SendStrangePacket", _noop)
_define(player, "ResetHorseSkillCoolTime", _noop)
_define(shop, "SetItemData", _noop)
_define(systemSetting, "SetSoundVolume", _noop)
for _name in ("ClearSlotCoolTime", "DisableScissorRect", "EnableScissorRect", "RestoreSlotCoolTime",
		"SetBaseDirection", "StoreSlotCoolTime", "TransferSlotCoolTime", "LoadImageFromFile"):
	_define(wndMgr, _name, _noop)
_define(wndMgr, "IsScissorRectEnabled", lambda *a: 0)
_define(wndMgr, "TEXT_BASEDIR_AUTO", 0)
_define(wndMgr, "TEXT_BASEDIR_LTR", 1)
_define(wndMgr, "TEXT_BASEDIR_RTL", 2)
_define(wndMgr, "TEXT_HORIZONTAL_ALIGN_ARABIC", 0)

# Font picked in the dev options (uidevoptions); applied before any text loads.
def _ApplySavedFont():
	if not hasattr(app, "SetPortFont"):
		return
	try:
		f = open("display.cfg", "r")
	except IOError:
		return
	try:
		for line in f.readlines():
			parts = line.split()
			if len(parts) == 2 and parts[0] == "font" and parts[1] != "default":
				app.SetPortFont(parts[1], 0)
	finally:
		f.close()

_ApplySavedFont()
