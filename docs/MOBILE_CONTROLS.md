# Mobile controls

How each desktop Metin2 input maps to touch on Android. Desktop keyboard and mouse
bindings are unchanged; the touch HUD (`android/tools/data-overlay/uimobilehud.py`)
only loads when the Android profile module `m2profile` exists.

Settings → Game options → **HUD** switches between **Mobile** (default on Android) and
**Desktop**. Mobile shows the touch HUD and hides the taskbar's buttons and quick slots,
keeping only the HP/SP/EXP gauges; Desktop restores the stock taskbar and hides the HUD.
The choice is saved in `mobilehud.cfg` next to `metin2.cfg`.

| Desktop | Touch |
|---|---|
| Arrow keys / WASD | Floating joystick (left half, appears under the thumb) |
| Left click on ground / target | Tap |
| Left click held | Hold finger still |
| Right drag (camera) | One-finger drag on the world; works together with the joystick |
| Mouse wheel / R, F (zoom) | Pinch (planned) |
| Space (attack) | Mobile HUD: hold the big sword button (bottom right). Desktop HUD: taskbar sword |
| 1-4, F1-F4 (quick slots) | Eight buttons in two arcs around the sword button; skill cooldown and active state shown on the button |
| Z / ` (pick up) | Hand button on the right edge, above the quick slots |
| I, C, V, N, B, M, L, H | Gear button (right edge, above pick-up) opens a wheel: Bag, Character, Skills, Quests, Emotes, Map, Chat log, Friends |
| Alt (show names) | Wheel: Names (toggles) |
| Ctrl+G (ride) | Wheel: Ride |
| Esc (system menu) | Wheel: Settings |
| Drag item/skill to a slot | Unchanged (drag with a finger) |

Planned next: pinch zoom, bigger hit areas on the stock windows (inventory, dialogs,
close buttons), long-press on a quick-slot button to move or clear it.
