#pragma once

//////////////////////////////////////////////////////////////////////////
// ### Default Ymir Macros ###
#ifndef LOCALE_SERVICE_EUROPE
#define LOCALE_SERVICE_EUROPE
#endif
#define ENABLE_COSTUME_SYSTEM
#define ENABLE_ENERGY_SYSTEM
#define ENABLE_DRAGON_SOUL_SYSTEM
#define ENABLE_NEW_EQUIPMENT_SYSTEM
// ### Default Ymir Macros ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### New From LocaleInc ###
#define ENABLE_PACK_GET_CHECK
#define ENABLE_CANSEEHIDDENTHING_FOR_GM
#define ENABLE_PROTOSTRUCT_AUTODETECT
#define ENABLE_SEQUENCE_SYSTEM
#define ENABLE_PCG_SEQUENCE // packet sequence bytes from pcg32(seed 0), matching m2dev servers

// #define ENABLE_PLAYER_PER_ACCOUNT5 // m2dev server sends 4 slots
#define ENABLE_LEVEL_IN_TRADE
#define ENABLE_DICE_SYSTEM
#define ENABLE_EXTEND_INVEN_SYSTEM
#define ENABLE_LVL115_ARMOR_EFFECT
#define ENABLE_SLOT_WINDOW_EX
#define ENABLE_TEXT_LEVEL_REFRESH
#define ENABLE_USE_COSTUME_ATTR

#define WJ_SHOW_MOB_INFO
#ifdef WJ_SHOW_MOB_INFO
#define ENABLE_SHOW_MOBAIFLAG
#define ENABLE_SHOW_MOBLEVEL
#endif
// ### New From LocaleInc ###
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// ### From GameLib ###
#define ENABLE_WOLFMAN_CHARACTER

// #define ENABLE_MAGIC_REDUCTION_SYSTEM
#define ENABLE_MOUNT_COSTUME_SYSTEM
#define ENABLE_WEAPON_COSTUME_SYSTEM
// ### From GameLib ###
//////////////////////////////////////////////////////////////////////////

/*
	###		New System Defines - Extended Version		###
*/

// if is define ENABLE_ACCE_SYSTEM the players can use shoulder sash
// if you want to use object scaling function you must defined ENABLE_OBJ_SCALLING
// #define ENABLE_ACCE_SYSTEM // not supported by the m2dev server protocol
#define ENABLE_OBJ_SCALLING

// if you want use SetMouseWheelScrollEvent or you want use mouse wheel to move the scrollbar
#define ENABLE_MOUSEWHEEL_EVENT

//if you want to see highlighted a new item when dropped or when exchanged
#define ENABLE_HIGHLIGHT_NEW_ITEM

/*
	###		New Debugging Defines
*/
#define ENABLE_PRINT_RECV_PACKET_DEBUG
#define ENABLE_CHAT															// Flag on Chat System
// #define ENABLE_EXTENDED_ITEMNAME											// Extended Item Name System
#define ENABLE_TARGET_INFORMATION_SYSTEM											// Target Information System
#define ENABLE_HEALTH_BOARD_SYSTEM													// Health Board System
// #define ENABLE_VIEW_TARGET_MONSTER_HP												// Target Hp Percent System
#define ENABLE_DAMAGE_BAR															// Damage Bar System
#define ENABLE_HEALTH_PERCENT_SYSTEM												// Target Hp Percent Sysetm
#define ENABLE_DS_GRADE_MYTH
#define ENABLE_EMOJI_UPDATE															// Emoji update.
#define ENABLE_CHANNEL_SWITCH_SYSTEM												// Channel Switcher System
#define ENABLE_CHANNEL_INFO_UPDATE

//////////////////////////////
#define ENABLE_CRASH_MINIDUMP

#define ENABLE_CRC32_CHECK
#define ENABLE_PETS_WITHOUT_COLLISIONS
#define ENABLE_SHOPS_WITHOUT_COLLISIONS
#define ENABLE_MOUNTS_WITHOUT_COLLISIONS
#define ENABLE_LOAD_ALTER_ITEMICON
#define ENABLE_SKIN_EXTENDED
// #define ENABLE_NO_MOUNT_CHECK
// #define ENABLE_SIMPLE_REFINED_EFFECT_CHECK
// #define USE_WEAPON_COSTUME_WITH_EFFECT
// #define USE_BODY_COSTUME_WITH_EFFECT
#define ENABLE_ATLASINFO_FROM_ROOT
#define ENABLE_NEW_ATLAS_MARK_INFO
// #define ENABLE_NEW_MOB_PROTO_STRUCT_20141125	// bleeding resistance 2014/11/25
// #define ENABLE_NEW_MOB_PROTO_STRUCT_20151020	// claw resistance 2015/10/20
#define ENABLE_NO_PICKUP_LIMIT
// #define __USE_CYTHON__

// #define __USE_EXTRA_CYTHON__
// #define ENABLE_DAEMONPROTECTION
#define ENABLE_PYLIB_CHECK
#define ENABLE_MILES_CHECK
#define ENABLE_QUEST_RENEWAL
#ifndef ENABLE_MOBILE_CONTROLS
#define ENABLE_MOBILE_CONTROLS
#endif
#ifndef ENABLE_OPENGL
#define ENABLE_OPENGL
#endif

#ifdef __ANDROID__
	#ifndef GrannyTypeSizeCheck
		#define GrannyTypeSizeCheck(expr) 
	#endif
#endif
