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
- The client connects to the channel address from `serverinfo.py`, not one the
  auth server hands out, so one tunnel port per channel core is enough.

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
