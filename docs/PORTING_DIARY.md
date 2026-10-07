# Metin2 Android porting diary

Running log of findings while porting the r10dev "OPENGL-ITJA" Metin2 client
(`r10dev.net OPENGL-ITJA/clientsource`) to Android, kept so later ports
(other platforms, other Win32/D3D8-era games) can reuse it.

## Stack used

| Piece | Source | Notes |
|---|---|---|
| Client source | Bahori35/metin2-android (this repo) | MSVC/Win32 + partial D3D8->GLES3 bridge (`EterLib/GrpOpenGL.*`) |
| Client data | d1str4ught/m2dev-client, last commit before its Python 3 conversion | packs read from external storage, not the APK |
| Server | d1str4ught/m2dev-server-src (`_IMPROVED_PACKET_ENCRYPTION_` variant, KEY_AGREEMENT=0xfb) + m2dev-server data | `python3 start.py 1` |
| Toolchain | NDK 25.2.9519653, platform 33, Gradle 8.1.1, JDK 17 | ABIs arm64-v8a, x86_64 (armeabi-v7a trips LZO asserts) |

Official Gameforge clients/servers are not an option: protocol and pack format
have moved on, and modified clients on official servers break the ToS.

## Build

```
cd "r10dev.net OPENGL-ITJA/android" && /opt/gradle-8.1.1/bin/gradle assembleDebug   # ~5 min
```

Prebuilt static libs in `Extern/lib/<abi>`: Python 2.7.18 (UCS4, ~25 builtin
stdlib modules), Crypto++ 8.2, LZO, libjpeg, libpng. Stubbed (proprietary, no
Android builds): Granny2 (`android_compat/granny_stub.cpp`, 59 functions),
DevIL, SpeedTree, Miles.

## Win32 -> Android shim (`android_compat/`)

- Fake `windows.h`/`winsock.h`/`io.h` + `win_stub.cpp`. Keep Windows widths:
  `LONG`/`DWORD` are 32-bit (`int32_t`/`uint32_t`) - never `long` on LP64.
- ~2000 initial clang errors; most were MSVC permissiveness (two-phase lookup,
  `for` scope, missing `typename`, implicit conversions, `__forceinline`, etc.).
- Python <-> C++ glue passed pointers as 32-bit ints (`PyInt`, `"i"` format).
  On 64-bit this truncates window/image handles -> crashes like
  `CWindow::Hide`. Use `PyLong`/`"L"`/`intptr_t` everywhere a handle crosses.
- `ShowCursor()` must return a running display count; callers loop until the
  count goes negative/positive.
- GDI fonts are emulated with Android system fonts (`gdi_font.cpp`).
- The game only renders when it believes the window is visible
  (`WM_ACTIVATE`/show messages never arrive) - force the visible state at start.
- Paths: Windows `\` separators and case-insensitive lookups must be normalised.

## Rendering (D3D8 fixed function on GLES3)

- Run the game loop on its own thread that owns the EGL context and a
  `SurfaceView`; blocking the GLSurfaceView thread means no frame is shown.
- DDS header is 124 bytes (not 128). DXT3/5 decoders had pointer-math overruns.
- FVF texcoord size bits: `D3DFVF_TEXCOORDSIZE2` is 0, SIZE3=1, SIZE4=2,
  SIZE1=3 at bits 16+2i; vertex stride depends on them.
- Terrain (`MapOutdoorRenderHTP.cpp`) needs two texture stages with
  `D3DTSS_TCI_CAMERASPACEPOSITION` texgen + `D3DTTFF_COUNT2` texture matrices.
- Audit every D3DX math helper in the shim before debugging shaders. Ours had
  placeholder `D3DXMatrixInverse` (returned input), `D3DXVec3TransformCoord`,
  `D3DXPlaneDotCoord`, `D3DXVec3Project/Unproject`; the camera-space texgen
  matrices were garbage until those were real.
- Render-to-texture must be real (FBO + depth renderbuffer). The terrain's
  last pass multiplies the framebuffer (`ZERO, SRCCOLOR`) by a character
  shadow render target; with no-op `SetRenderTarget` that texture stayed
  black and blackened all terrain. When rendering to an FBO, flip clip-space Y
  so texture row 0 is D3D's top and use an unflipped `glViewport`.
- `.wtr` water heights are serialized 32-bit `long`; on LP64 read `int32_t`.
- Debug technique: `adb shell setprop debug.m2.solid N` switches shader debug
  modes at runtime (skip lit terrain pass, show vertex colour, stage-0/1
  texture/coords) to bisect a multipass effect without rebuilding.
- Lesson: add fixed-function features one at a time with a screenshot after
  each; a big all-at-once shader rewrite produced corrupt geometry that was
  hard to bisect.

- Text: the font atlas path was fine; `MultiByteToWideChar` in the shim only
  handled CP_UTF8/CP_ACP and returned 0 for the game's code page (1252/1254),
  so every string became zero glyphs. It also ignored `cbMultiByte` (mbstowcs
  reads to NUL). Implement real UTF-8 + Windows single-byte code pages.

## Networking / protocol traps

- POSIX non-blocking connect: `EINPROGRESS` is success; `select()` nfds must be
  `sock+1`; check `SO_ERROR` once writable.
- Server sends packet sequence bytes from a PCG32 stream, not the client's
  static table; reimplemented and checked for 100k values.
- When the cipher activates, bytes already in the receive buffer must be
  decrypted too.
- Feature flags change packet layouts. `ENABLE_ACCE_SYSTEM` and
  `ENABLE_PLAYER_PER_ACCOUNT5` had to be off for m2dev (4 slots, no acce) or
  LOGIN_SUCCESS4 parsed the game-core address as 0.0.0.0.
- Packet-size diffing: build tiny host programs that `printf` header id +
  `sizeof` for every registered packet on both sides (`CMainPacketHeaderMap`
  on the client). Header names differ between trees; compare by number.
- Best ground truth: enable the server's `SENT HEADER` log in
  `DESC::Packet/BufferedPacket` and an Android-side per-packet log in
  `CPythonNetworkStream::CheckPacket`, then diff the two streams. Note dynamic
  packets are often sent as a buffered 3-byte header followed by a raw payload
  `Packet()` call, so the server log shows the payload's first byte as a
  "header".
- Misframing symptoms are not always "Unknown packet header": the client hung
  at 100% CPU inside `RecvDuelStartPacket` -> `InsertDUELKey` because garbage
  bytes were parsed as a DUEL_START with a huge size. `debuggerd -b <pid>`
  gives the game-thread stack without killing the app.

## In-game packet/data fixes (quest, property)

- Feature flags must match the server, not the client tree's defaults:
  `ENABLE_QUEST_RENEWAL` adds a `WORD c_index` to `packet_quest_info`; the
  m2dev server does not send it, so every quest packet desynced the stream
  (symptom: "Unknown packet header: 105" a few packets later). Disabled it.
- LP64 again: `quest.GetQuestData` returned the icon `CGraphicImage*` via
  `Py_BuildValue("i")`, truncating it; `wndMgr.SetSlot` then crashed in
  `CReferenceObject::AddReference`. Grep every `Py_BuildValue` that passes a
  pointer and use `"l"` + `(long)(intptr_t)`.
- `FindFirstFile` shim: Windows `*.*` matches names without dots (directories)
  and paths are case-insensitive; POSIX `fnmatch("*.*")` and `opendir` are
  not. Map `*.*` -> `*`, use `FNM_CASEFOLD`, resolve directory case.
- Text data extracted from git-hosted client repos is LF-only;
  `CProperty::ReadFromMemory` required `\r\n` after the `YPRT` FourCC, so all
  ~750 `.prd` properties failed ("CArea::LoadObject Property(...) Load ERROR",
  no trees/buildings). Accept both line endings.

## Server ops

- DB crashed in `mysql_set_character_set()` when started before MySQL was up;
  wait for readiness.
- Replacing a running binary: `cp` fails with "Text file busy"; copy to a temp
  name and `mv` over it.
- `start.py` can race on ports; check `ss -ltnp` and restart a missing core by
  hand from its directory.
- Test account `test` / `test123`; auth 11000, ch1 cores 11011-11013, db 9000.

## Emulator workflow

- Data in `/storage/emulated/0/Android/data/com.metin2.client/files/` (push
  with adb). Client log: `syserr.txt` there + `adb logcat -s Metin2Mobile`.
- Screenshots: `adb exec-out screencap -p > shot.png`.

## Touch input -> movement

- Path: `MainActivity.onTouchEvent` -> JNI `touchEvent` -> queue in
  `CMSApplication` -> `CPythonApplication::OnTouchEvent` -> `OnMouseMove` +
  `OnMouseLeftButtonDown/Up` -> `CWindowManager` -> `game.py`
  `OnMouseLeftButtonDown` -> `player.SetMouseState` -> `__OnPressSmart` ->
  ground pick -> `SendCharacterStatePacket`.
- Trap 1: Android hands physical surface pixels (2148x1080 on the emulator);
  the client works in its logical resolution (1024x768). Unscaled taps fell
  outside every UI window, so nothing ever reached `game.py`. Scale in
  `OnTouchEvent` by `m_dwWidth / surfaceWidth`.
- Trap 2: the main loop polls `GetCursorPos()` every frame and calls
  `OnMouseMove` with it, and the picking ray is built from that. A stub that
  returns (0,0) silently overrides each touch. Feed the scaled touch position
  back through `GetCursorPos`.
- Granny root motion: `.msa` `Accumulation` and the GR2 track group's
  `LoopTranslation` + `Flags & 3` carry per-loop displacement (run.gr2:
  0,-300,0 per 0.667 s). `GrannyUpdateModelMatrix` must apply it scaled by
  elapsed/duration and control weight; a copy-only stub keeps the actor in place.
- Verify movement server-side, not by pose: `player.player` x/y only change
  after the game save event (120 s) plus the db cache flush (up to 7 min), or
  on logout + flush. Verified: 964548,276496 -> 967275,274106.
- Unresolved: `SEQUENCE ... mismatch header 254` (PONG) in core syserr; it did
  not recur in the move session. Do not disable the server check.

## Open items (as of this entry)

- Terrain/text rendering in GL bridge; Granny `.gr2` loader (models invisible);
  remaining packet layout mismatches (item set/del, shop, target, skill level);
  combat/inventory/shop/chat validation; PONG sequence mismatch.

## UI rendering and Android keyboard

- Upside-down icons, scrambled dock and map tiles had one root cause in
  `EterImageLib/TGAImage.cpp`. The Android loader decodes TGA with stb_image,
  which already honours the TGA origin bit and returns rows top-down. It then
  built a fake header with `desc = 0x08` (no `IMAGEDESC_TOPLEFT`), so
  `CTGAImage` called `FlipTopToBottom()` again. Every TGA came out mirrored
  vertically, so sub-rects cut from shared sheets (dock slots, map tiles,
  potion icons) sampled the wrong region. Fix: `desc = 0x08 | IMAGEDESC_TOPLEFT`.
  DDS/JPEG paths were already correct.
- Lesson for other ports: when you swap in a decoder library, check whether it
  already normalises orientation before keeping the engine's own flip.
- Soft keyboard: keep one owner for visibility (`PythonApplication::Process`
  syncs from IME capture state). `GameView.onCheckIsTextEditor` and
  `onCreateInputConnection` must return false/null while not capturing, or the
  IME pops up on any hardware key. Gboard composes text and never commits it to
  a non-full-editor `BaseInputConnection`, so use
  `TYPE_TEXT_VARIATION_VISIBLE_PASSWORD` to get direct commits and a working
  Send action. Call `requestFocus` before `restartInput`.

## Android targeting and combat

- The existing Windows mouse path already maps correctly to Android taps once
  touch coordinates and cursor polling are fixed: tap an actor to send
  `HEADER_CG_TARGET`; tap/hold it again to drive the normal space-key attack
  state and `HEADER_CG_ATTACK`. No Android-specific combat packet is needed.
- Do not add an Android attack button. The taskbar sword menu already offers
  auto attack (`player.MBF_AUTO`); one tap on a mob then keeps attacking it via
  the original game path (cooldowns, animation timing, CRC fields).
- Verify the whole exchange from both ends. Against a level 8 Cursed Wolf the
  client sent target and attack packets, received HP 100 -> 61 -> 22 -> 0,
  received damage values 168/171/168, then target-clear and dead packets. The
  hostile wolf also dealt repeated server-authored damage to the player.
- A previously observed PONG sequence mismatch did not recur after a clean
  server restart; keep server sequence validation enabled and re-check during
  longer sessions rather than bypassing it.

## Touch picking (no hover phase)

- A mouse hovers before it clicks, so the engine's per-frame pick (actor under
  the cursor) is already up to date when the button goes down. A touch moves
  and presses in the same instant, so the press used last frame's pick result
  and always hit the ground.
- Fix: queue touch events; on a press, first deliver a move, then hold the
  press back until two rendered frames have rebuilt the pick ray (signalled
  right after `SetCursorPosition` in the 3D render path), with a 1.5 s timeout
  so menus without a 3D frame never stall.
- Do not rebuild the pick ray from the input handler. Outside the 3D frame the
  projection/view matrices belong to the 2D UI pass, so the ray points at the
  sky and nothing is picked.
- Verified: tapping a Metin stone (vnum 8001) with sword auto mode sent target,
  then repeated attacks; server returned damage ~204 per hit, HP 100% -> 0%,
  target clear and dead packets. Spawned guards were also hit and killed.

## Test bundles and profiles

- A bundle = APK built from `android/profiles/<name>.properties` + a versioned
  client-data zip. `tools/make_bundle.sh <profile> <build>` produces both;
  `-Pm2profile=<name>` selects the profile (`-Pm2abi` still overrides the ABI list).
- Profile keys become `BuildConfig` fields: server mode/host/ports, data URL and
  data version. At launch the app resolves the host, writes `m2profile.py` into
  the data dir, and the data's `serverinfo.py` imports it (falls back to its own
  values if absent). Changing servers = new profile, no code or data edit.
- The APK downloads and unzips the data on first launch (streamed, no temp copy;
  version marker written last so an interrupted download retries). A `.txt` data
  URL is a pointer file, so the archive host can move without a rebuild.
- Gotchas: Android 9+ blocks cleartext HTTP unless `usesCleartextTraffic`; the
  extracted data was missing the Python 2.7 stdlib `.py` files (posixpath etc.)
  that the dev emulator had from earlier manual pushes. A fresh-install test is
  the only way to catch that; the bundle script now overlays them into `lib/`.
- Tunnels for phone testing: raw TCP via bore.pub for auth/channel (game traffic
  is tiny), but bore.pub gave ~70 KB/s, too slow for 1 GB of data. A cloudflared
  quick tunnel served it at tens of MB/s; its random URL goes into the pointer file.
- The client's first channel connection uses the address from `serverinfo.py`, but
  after `LOGIN_BY_KEY` the core sends the address of the core that hosts the
  character's map (see "Phone kicked back to server select" below).

### Bundled-data APKs, pinned tunnel IPs, soft-keyboard shift

- `m2.dataBundled=true` packs the versioned data zip into the APK as `assets/m2data.zip`. `make_bundle.sh` hard-links it into `$M2_BUNDLE_OUT/assets-<ver>/` and passes `-Pm2dataAssets`. aapt `noCompress 'zip'` keeps the asset stored, so `AssetManager.openFd()` gives its length for the progress bar. The engine needs loose files through stdio, so the zip is still extracted to external files on first launch. That took under a minute on the emulator, versus the download. A 1 GB APK installs fine through adb.
- On a user's network, `bore.pub` resolved to 81.99.162.48, while our tunnels are on 159.223.110.159, and the connection timed out. Profiles therefore pin the bore server by IP. Lesson: never rely on a third-party relay's DNS for test bundles.
- Soft keyboard: the activity uses `adjustNothing`, so the visible frame never shrinks and `getWindowVisibleDisplayFrame` doesn't see the IME. On API 30+, read `WindowInsets.Type.ime()` from the decor view's insets listener instead. Native passes the focused edit window's bottom (as a fraction of the screen) with `setKeyboardVisible(boolean, float)`. Java then translates the game view up so that bottom, plus a 12% margin to cover the next field (password) and the button, sits above the keyboard. Touch Y is corrected by `getTranslationY()`, because the activity receives window coordinates.

## Embedded server (offline profile)

- Layout: `server/` builds the pinned m2dev-server-src (66068c1, unpatched,
  cloned by Gradle into `server/.src` or `-Pm2serverSrc=`) with NDK r25 into
  PIE executables `libm2db.so` / `libm2game.so`, packaged as jniLibs so they
  land executable in `nativeLibraryDir` (exec from app data is blocked on
  API 29+). `m2.serverMode=embedded` adds them; `remote` builds are unchanged.
- Database: no MariaDB on the phone. `server/sqlite-mysql` is a drop-in
  `libmariadbclient` (same `mysql.h` C API) over SQLite 3.50: one file per
  logical DB (`account/player/common/log/hotbackup.sqlite3`), all ATTACHed so
  `player.item` style qualified names work. Queries are rewritten from MySQL:
  NOW()/UNIX_TIMESTAMP/FROM_UNIXTIME/PASSWORD() as SQL functions, REPLACE /
  INSERT ... SET / ON DUPLICATE KEY UPDATE / INSERT DELAYED|IGNORE, SHOW
  TABLES / SHOW CREATE TABLE, DATE_ADD/TIMESTAMPDIFF, ENUM/SET (metadata in
  `_m2_enum`, numeric index semantics via triggers), backslash escapes.
  `server/tools/mysql2sqlite.py` converts the exact `m2dev-server/sql` dumps.
  The DB server boots `hotbackup` too; an empty file is enough.
- Persistence: seed DBs ship in the external server pack
  (`files/server/sqlite-seed`); the app copies them once to internal
  `files/m2server/sqlite` and never overwrites, so accounts/characters/items
  survive restarts and pack re-pushes. Cache flush cycles are cut to 30 s in
  the pack so a killed app loses little; the launcher SIGTERMs the cores
  (they save players) before the db on exit.
- Test account: `test` / `test123` (MySQL double-SHA1 in `account.account`),
  created only in generated seed files; nothing credential-like is committed.
- Bionic/NDK breakage and fixes:
  - libc++ in r25 has no `<source_location>` (C++20, used by the log macros):
    tiny builtin-based replacement in `server/compat/android`.
  - `getifaddrs`/`freeifaddrs` only exist from API 24: resolved via `dlsym`
    at runtime, otherwise no interfaces; the cores always get `-I 127.0.0.1`.
  - Lua 5.0 aborted at boot with `FORTIFY: strchr: prevented read past end of
    buffer` (ltable/lgc reads string bytes past the `TString` header, which
    FORTIFY's object-size check rejects). FORTIFY is off for `liblua` only.
  - glibc host builds lack `strlcpy/strlcat` (bionic has them): BSD fallback
    header force-included on the host only. SQLite needs `_GNU_SOURCE` for
    `mremap`.
  - `qc` runs on the host while building the pack (quests are data, not code).
  - Clients connected but never got `GC_HANDSHAKE` ("handshake session has
    expired"). libthecore's POSIX backend calls `select(0, ...)` (fine on
    Winsock, which ignores nfds); Linux then checks no fds and leaves the sets
    untouched, so every socket looks readable, `fdwatch_check_event` always
    returns READ and output is never flushed (also a 100% CPU busy loop).
    `compat/select_nfds.h` is force-included into libthecore only and passes
    `FD_SETSIZE` when nfds is 0; the pinned source stays unpatched.
  - Pin is 66068c1, not 0cc595b: db87d06 (between them) replaced the Crypto++
    KEY_AGREEMENT (0xfb/0xfa) with a libsodium KEY_CHALLENGE (0xf8/0xf9/0xf7)
    the client doesn't speak; auth sent the challenge and timed out.
    66068c1 is what the Linux server the client was tuned against runs.
  - cryptopp's CMake does `add_compile_options("${CMAKE_CXX_FLAGS}")` (one
    quoted argument); harmless on the host where it is empty, fatal with the
    NDK flags. `CMAKE_CXX_FLAGS` is cleared around its `add_subdirectory`.
  - The Linux setup's `QueryLocaleSet` connect-wait patch isn't needed: the
    SQLite shim is connected synchronously and `mysql_set_character_set` is
    a no-op.
- Server data pack: `server/tools/make-server-pack.sh <m2dev-server>
  <m2dev-server-src> <out>` (share/conf,data,locale,mark + compiled quests +
  seed DBs, ~116 MB), `push-server-pack.sh <out>` puts it at
  `files/server`. Logs go to `files/server/logs/<process>.{out,syslog.log,syserr.log}`.
- Process model: `EmbeddedServer` starts db -> auth -> channel1 core
  carrying all three empires' normal maps (upstream splits them over cores
  1-3; with only empire 1's maps the seeded empire-3 character got
  "cannot find server for mapindex 41"), waiting until each port
  accepts, before the game view is created; stale processes from a killed
  app are found via their `pid` files and terminated.


### Phone kicked back to server select after Start; character-select offset

- Symptom: on the phone (tunnel profile), Start on character select returned to the server list. Server logs showed `LOGIN_BY_KEY` on core1, then the socket closed with no `player_select`. The emulator worked because it can reach the LAN address directly.
- Cause: the login-success packet carries each character's game server address. The character's map (41) is hosted on core3, so core1 advertised `172.16.255.2:11013`. The phone can't reach that, and a failed connect returns to the server list. Warps (`HEADER_GC_WARP`) do the same.
- Fix: tunnel every core (`4711x -> 1101x`) and add the profile key `m2.gamePortOffset`. When it is nonzero, Java sets `M2_GAME_HOST`/`M2_GAME_PORT_OFFSET` with `Os.setenv`. `CNetworkStream::Connect(DWORD, port)` then swaps in the profile host and shifts the port. A fully tunnelled x86_64 build of the phone profile then reached `ENTERGAME` on core3.
- Character select: `grp.SetViewport` takes fractions of the screen, and the GLES layer multiplied them by the logical 1024x768 back buffer instead of the real surface. Android now uses `g_iAndroidSurfaceWidth/Height`.
- Lesson: when a phone-only bug looks like a crash, read the server logs for the session first. "Back to the server list" was a failed connect, not a native crash.

### Iteration loop

- Phone profiles build arm64 only; Gradle passes ccache as the CMake compiler launcher when it exists.
- `m2.devUrl`/`m2.updateChannel`: `make_bundle.sh` writes `latest-<channel>.txt`. On launch the app offers newer builds and opens the APK URL. It also posts the previous run's `crash.txt`, `syserr.txt`/`stderr.txt` tails and the app's own logcat to `POST /crash`.
- The native crash handler (`AndroidMain.cpp`) writes signal, fault address and an `_Unwind_Backtrace` with `dladdr` to `crash.txt`. Symbols for each build are copied to `$M2_SYMS/<name>-<build>`.
- To reproduce phone profiles on the x86_64 emulator, use `-Pm2abi=x86_64`. When moving the install from arm64 to x86_64, `adb install --abi x86_64` an APK that contains both ABIs first, otherwise the install fails with "Error deriving application ABI".

### Touch felt ~1 s late on login/select screens

- The two-frame press delay (see touch targeting) counted frames in `RenderGame`, which only runs in the game phase. On login and select it never counted, so every press waited for the 1500 ms safety timeout. The end of every rendered frame in `Process()` now counts a frame when `RenderGame` did not. syserr also logs update/render FPS every 10 s, so phone performance can be read from crash/log uploads.

### Backgrounding, legible UI, joystick and camera drag (builds 19–23)

- Lifecycle: the native game thread and its network connection are static and outlive the
  activity's surface. `surfaceDestroyed` -> `setSurface(null)`: the game thread destroys its
  EGL window surface and keeps ticking, and Present sleeps ~33 ms instead of swapping. A new
  surface is attached on the game thread at the next Present. Repeated `surfaceChanged` calls
  with the same `ANativeWindow` are ignored, otherwise each resume re-created the surface 2–3 times.
- `eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)` fails with `EGL_BAD_MATCH` (0x3009)
  on the emulator's driver (no surfaceless contexts). Bind a 1x1 pbuffer while detached instead,
  which means the config needs `EGL_PBUFFER_BIT`.
- Without a foreground service, a backgrounded game sits at oom_adj 700 and the low-memory
  killer took it after ~30 s on a 2 GB emulator, server processes included. `GameSessionService`
  (an ongoing "Game running. Tap to return." notification) keeps it at 50–200. Started in
  `startGame()`, stopped when the activity finishes.
- An obsolete activity must not stop the embedded server: `singleTask` plus a generation counter,
  and the server stop runs only if no newer activity has started.
- Legibility: rendering the 1024x768 logical UI stretched to 2148x1080 made text tiny and wide.
  `CPythonSystem::FitUIToAndroidSurface()` sets logical height 600 and width = 600 * aspect, so
  text is ~1.3x larger and square. The UI scripts already anchor to SCREEN_WIDTH/HEIGHT, so
  login/select/game layouts followed without script edits (the server dialog overlaps the logo
  slightly).
- Touch model (`CPythonApplication::OnTouchEvent`): a touch that starts over a UI window acts as a
  plain left button. On the world, a quick tap is replayed as a synthetic down/up through the
  two-frame hover deferral (so target picking still works), a press held still ~350 ms becomes a
  held left button (walk/attack), and a drag past a small slop becomes `CCamera::DragBy(dx, dy)`
  (the right-mouse-drag equivalent) without moving the character.
- Joystick (`JoystickView`): drives DPAD up/down/left/right key events, i.e. the engine's
  arrow-key movement, so walking is camera-relative exactly as on desktop. 8 directions via
  thresholds, with a deadzone. Native code shows it only in the game phase
  (`AndroidSetGameControlsVisible`), and it releases its keys on pause.

## Browser port (Emscripten/wasm)

Same client, second adapter. The platform seams the Android port introduced
(`clientsource/platform/` — `M2Plat` for windowing/GL/input/env and `M2Net`
for the byte stream) paid off: the web adapter is two files
(`platform/web/WebMain.cpp`, `platform/web/M2WebNet.cpp`) and no game code
changed. `M2_PORT` (`__ANDROID__ || __EMSCRIPTEN__`) selects the port paths.

### Build

```
source ~/emsdk/emsdk_env.sh        # emscripten 6.0.11
"r10dev.net OPENGL-ITJA/web/build.sh"   # -> build-web/metin2_web.{js,wasm,html}
```

- `M2_TARGET=web` in `clientsource/CMakeLists.txt`; all flags live in
  `web/emflags.cmake`. `-std=c++14` (the tree predates C++17).
- wasm extern libs (Python 2.7, Crypto++, LZO) in `Extern/lib/web` +
  `Extern/include/Python2-web`, built by `tools/wasm-deps/build.sh` from
  the same sources as the Android prebuilts.
- `-sUSE_PTHREADS -sPROXY_TO_PTHREAD -sOFFSCREEN_FRAMEBUFFER
  -sEMULATE_FUNCTION_POINTER_CASTS` are the load-bearing flags. zlib/png/
  jpeg come from emscripten ports, not the Extern prebuilts — see jpeg ABI
  below.

### PROXY_TO_PTHREAD changes everything

main() runs on a worker thread, and Emscripten proxies every libc call to
the main (UI) thread synchronously. Consequences, each of which cost a
debugging round:

- **No lazy files.** The FS lives on the main thread where sync XHR is
  banned, so `FS.createLazyFile` is out. `web/shell.html` fetches
  `manifest.json` and downloads every data file into MEMFS under `/data`
  before releasing `main()` (`addRunDependency`/`removeRunDependency`;
  `?m2_boot=1` waits for all ~51k files, the default boot tier is enough
  for the login screen and the rest streams in the background).
- **Frame commit.** The engine drives its own loop on the worker, but GL
  renders into an offscreen FBO that only reaches the canvas at rAF
  boundaries — which never happen. `M2Plat::PresentFrame` must post
  `GL.blitOffscreenFramebuffer(GL.currentContext)` via
  `MAIN_THREAD_ASYNC_EM_ASM` or you get a black canvas while the game runs
  at 60 fps.
- **Input starvation.** `emscripten_set_*_callback` registrations proxy
  to the main thread, which queues each event into the worker's mailbox —
  and the engine's busy loop never yields, so clicks and keys pile up
  forever. `PresentFrame` calls
  `emscripten_current_thread_process_queued_calls()` once per frame to
  drain it.
- **Worker `location` is the worker script.** `location.host`/`search` in
  worker code return the wasm worker URL, not the page. Anything that
  needs the page origin/query (the ws bridge URL, `m2_*` query params)
  must go through `MAIN_THREAD_EM_ASM`.
- **`canvasX`/`canvasY` are dead.** Current emscripten never fills them on
  `EmscriptenMouseEvent`/`EmscriptenTouchPoint` (deprecated with
  `Module['canvas']`); use `targetX`/`targetY`.
- **fp-cast traps.** Win32-era code calls function pointers through
  wrong-type casts; wasm traps with "null function or function signature
  mismatch" inside `system.py` without `-sEMULATE_FUNCTION_POINTER_CASTS`.
- **libjpeg ABI.** The Extern headers declare a 456-byte
  `jpeg_decompress_struct`; the emscripten port builds 488 bytes
  (`JPEG parameter struct mismatch`). Under `__EMSCRIPTEN__` include the
  port's `<jpeglib.h>`, not `Extern/include/libjpeg`.
- **Python stdlib.** The embedded 2.7 has ~25 builtin modules; `system.py`
  needs `os`/`posixpath`/`traceback`/… Copy CPython 2.7.18 `Lib/*.py` +
  `encodings/` into the staged data `lib/` and set
  `PYTHONPATH=/data/lib:/data` before `Py_Initialize`.

### Networking: ws -> tcp bridge

Browsers have no TCP. `M2WebNet` implements `M2Net` on
`emscripten_websocket_*` (created on the main thread) and connects to
`ws(s)://<page-origin>/ws?target=<host>:<port>`. `web/serve.py` accepts the
upgrade, dials the target TCP and pipes bytes both ways (masked client
frames -> raw tcp; tcp -> unmasked binary frames). `M2_WS_BRIDGE` env /
`?m2_ws_bridge=` overrides the bridge URL; an explicit `target=` inside it
wins over the appended one, which is how the transport was verified
against a local dummy endpoint. For production, `web/ws_bridge.py` is the
standalone asyncio version with an `--allow` list.

### Verifying headless

`/home/ubuntu/m2webtest/drive.py` (not in repo): pychrome CDP script —
loads the page, captures screenshots/console/syserr.txt, and executes a
timed action list (`click`/`type`/`key`/`eval` ops). Synthetic
`MouseEvent` dispatch on the canvas works fine for the game (it sees
`clientX`); `Input.dispatchKeyEvent type=char` produces real `keypress`
events with `charCode` for typing into the login fields. Boot to a live
ws connection plus a scripted server-select/OK/type/Connect run is the
smoke test.

### Serverless: db + game servers as wasm in the same page

`local.html` is the fully in-browser build: no ws bridge, no backend —
the two server binaries (`m2dev-server-src` db + game) compile to wasm
unchanged and run as emscripten pthread workers inside the page, next to
the client. Three binaries, one page, zero network after first load.

- **Build**: `server/CMakeLists.txt` gains an `EMSCRIPTEN` block —
  `m2db.{js,wasm}` and `m2game.{js,wasm}` with `PROXY_TO_PTHREAD`,
  `FORCE_FILESYSTEM`, `ALLOW_MEMORY_GROWTH`; the sqlite-mysql shim already
  solved the DB dependency for Android, and `fdwatch` falls back to
  `select()`. Only patch to upstream source: `server/m2dev-server-src-web.patch`
  (~110 lines) rerouting libthecore `socket_*` calls through the m2lp
  facade and pinning `g_szPublicIP = 127.0.0.1`.
- **Loopback transport**: `server/web/m2lb_bridge.cpp` implements
  socket/bind/listen/accept/connect/send/recv/select on ONE 8 MB
  `SharedArrayBuffer` — listener registry + 32 conn slots, each with two
  128 KB byte rings. Every worker sees the same SAB (posted by the page
  on worker start; pthreads receive it because they share the parent's
  memory object). All synchronous — no postMessage on the data path, so
  the transport can't starve like the input mailbox did.
- **Staging**: `web/srv_worker.js` wraps each server worker: waits for the
  SAB, mounts `srv-share.m2pack` (map/quest data) into MEMFS and
  `srv-sqlite.m2pack` (seed DBs) into IDBFS so characters persist, writes
  `CONFIG`, then loads the module script. `server/tools/make-server-pack.sh`
  builds those packs from an m2dev-server share dir + seeded sqlite files.
- **Client side**: `M2WebNet` picks the loopback when `Module.m2lbSab`
  exists — same `M2Net` port, transport invisible to the game.
- **Channel boot is slow**: the channel game worker loads map
  `server_attr` blobs (LZO) inside its io_loop before it accepts —
  ~2-3 min before :11011 answers. `local.html` gates engine start on a
  `m2chan` run-dependency that polls the SAB listener table until both
  :11000 (auth) and :11011 (channel) are bound, so the client never
  connects into a server that can't accept yet.
- **Accounts**: seed accounts live in the sqlite `account` DB —
  `test`/`test123` works out of the box.
- **Auto-login**: `?m2auto=1` writes a `loginInfo.py` (addr/port + id/pwd
  + autoLogin/autoSelect) — deterministic scripted login for headless
  tests, no board clicking needed.

- **Verified in-world**: `test`/`test123` -> server select -> char select
  -> spawn in Pyungmoo Area; NPCs/minimap/quest list/quickslots render,
  click-to-move + arrow keys walk. Mobile HUD (floating joystick zone,
  attack button, quickslot rings, soft-keyboard bridge) matches Android.
- **wasm pitfalls found the hard way**:
  - emscripten pthread stacks are 64 KB — `BYTE abComp[maxMemSize]`
    (~70 KB) in `SECTREE_MANAGER::LoadAttribute` silently smashed the
    stack and froze the channel worker mid-map-load (no exception, no
    console error). Heap-allocate on `__EMSCRIPTEN__` — same fix shape
    as the `_MSC_VER` path it already had.
  - `clang -O2` wasm codegen miscompiles the table-driven DXT1/DXT3/DXT5
    decoders (produced solid-white textures — white map window). Plain
    byte-index loops survive.
  - A `SharedArrayBuffer` only reaches an emscripten pthread via
    postMessage — expandos and `Module` don't survive structured clone,
    and emcc's init message carries only memory+module. The worker
    wraps `self.Worker` to post `{m2:'sab'}` before emcc's `{cmd:1}`.

## Gate, intro UI scale, and text size (builds 24+)

- **Passphrase gate**: `web/m2encrypt.py` AES-GCM-encrypts every
  `.m2pack` (PBKDF2, salt `m2gate-v1`); `m2packSetPassphrase()` decrypts
  in-page. Static hosts can't do auth — encrypting the payload IS the
  gate. `?m2pw=` or a sessionStorage-remembered passphrase skips the
  prompt so in-app restarts don't re-ask.
- **COI on static hosting**: `SharedArrayBuffer` needs
  `Cross-Origin-Embedder-Policy: require-corp`, which hosts like
  devinapps can't set — `web/coi-sw.js` is a service worker that
  injects the headers on every response.
- **display.cfg survives restarts**: MEMFS is rebuilt from packs each
  boot, so `RestartApp` stashes `/data/display.cfg` to localStorage
  before `location.reload()` and bootReady writes it back.
- **Intro UI-size widget** (`uiscale.py`, attached to login/empire/
  select/create): -/+ buttons + a click-anywhere slider row for UI
  size and one for text size + apply. Needed because ui_scale shrinks
  the logical canvas — boards laid out for ~600 logical px slide off
  the physical screen at 1.5x. The login/empire/select boards are also
  clamped to the canvas in uiscript so controls can't leave the screen.
- **font_scale** (display.cfg, 1.0-1.6): multiplies `.fnt` atlas
  generation size in `CGraphicText::OnLoad` — bigger rendered text,
  not magnified pixels (which is all ui_scale does for glyphs).
- Debug tale: plain `ui.Window` overlays and `ui.Button` hit-testing
  are fine, but the physical-px window (~0.75x logical) makes hand-aimed
  test clicks land ~10px off — the "dead slider" was a coordinate bug,
  not an input bug. Verify handlers with a file-writing probe, not
  screenshots.
