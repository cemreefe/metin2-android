# Mobile controls

How each desktop Metin2 input maps to touch on Android. Desktop keyboard and mouse
bindings are unchanged; the touch HUD (`android/tools/data-overlay/uimobilehud.py`)
only loads when the Android profile module `m2profile` exists.

| Desktop | Touch |
|---|---|
| Arrow keys / WASD | Floating joystick (left half, appears under the thumb) |
| Left click on ground / target | Tap |
| Left click held | Hold finger still |
| Right drag (camera) | One-finger drag on the world; works together with the joystick |
| Mouse wheel / R, F (zoom) | Pinch (planned) |
| Space (attack) | Taskbar sword (auto-attack) |
| 1-4, F1-F4 (quick slots) | Eight big buttons around the pick-up button, bottom right; skill cooldown and active state shown on the button |
| Z / ` (pick up) | Big "Pick up" button, bottom right |
| I, C, V, N, B, M, L, H | Menu button (right edge) opens a wheel: Bag, Character, Skills, Quests, Emotes, Map, Chat log, Friends |
| Alt (show names) | Wheel: Names (toggles) |
| Ctrl+G (ride) | Wheel: Ride |
| Esc (system menu) | Wheel: Settings |
| Drag item/skill to a slot | Unchanged (drag with a finger) |

Planned next: pinch zoom, bigger hit areas on the stock windows (inventory, dialogs,
close buttons), long-press on a quick-slot button to move or clear it.
