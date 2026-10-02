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

## Open items (as of this entry)

- Terrain/text rendering in GL bridge; Granny `.gr2` loader (models invisible);
  remaining packet layout mismatches (item set/del, shop, target, skill level);
  touch input and gameplay validation.
