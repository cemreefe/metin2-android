# Self-hosting guide: server, client data and the Android client

This guide covers everything needed to run the Android port without outside help:
where each part comes from, how to build and run a compatible server, how to
prepare the game data, and how to build an APK that connects to your server.

Paths below are relative to this repository unless they start with `~`.
`ANDROID=r10dev.net OPENGL-ITJA/android` is the Android project and
`CLIENT=r10dev.net OPENGL-ITJA/clientsource` is the C++ client.

## 1. The parts and where they come from

| Part | Source | Pinned revision | Notes |
|---|---|---|---|
| Android client (this repo) | https://github.com/cemreefe/metin2-android | tag `mvp` = first playable build | Fork of https://github.com/Bahori35/metin2-android |
| Server source | https://github.com/d1str4ught/m2dev-server-src | `66068c1` | Newer commits use a different handshake (see 1.1) |
| Server runtime (configs, SQL, quests, map data) | https://github.com/d1str4ught/m2dev-server | `ad30738` | |
| Client data (maps, models, UI scripts) | https://github.com/d1str4ught/m2dev-client | `98d2c1af` | Not committed here; see 1.2 |
| Python 2.7 standard library | https://www.python.org/ftp/python/2.7.18/Python-2.7.18.tgz | 2.7.18 | Copied next to the data |
| Native libs (python2.7, cryptopp, lzo, jpeg, png) | prebuilt in `r10dev.net OPENGL-ITJA/Extern/lib/android/<abi>` | | Rebuild with `$ANDROID/tools/extern/*.sh` |

### 1.1 Protocol compatibility

The client speaks the m2dev protocol as of server commit `66068c1`
(key agreement headers `0xfb`/`0xfa`). From
`0cc595bf097dcf89ddba60615e56fe790823cd22` onwards the server switched to a
libsodium/XChaCha20 handshake (`0xf8`) that this client does not implement.
Using a newer server gets you disconnected right after the login screen. If you
want a newer server, the client's handshake in `CLIENT/UserInterface/` has to be
ported first.

### 1.2 Data and licensing

The game assets belong to Webzen/Gameforge. The m2dev repositories publish them,
but this repository never contains them: no packs, no extracted data, no APKs
with data inside. Keep it that way. Build bundled-data APKs only for private use.

## 2. Running the server (Linux)

Tested on Ubuntu 22.04 with g++ 11, CMake 3.22 and MariaDB 10.6.

### 2.1 Build

```bash
sudo apt install build-essential cmake git mariadb-server python3
git clone https://github.com/d1str4ught/m2dev-server-src ~/m2dev-server-src
cd ~/m2dev-server-src && git checkout 66068c1
git apply <this repo>/server/m2dev-server-src-linux.patch
mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . -j"$(nproc)"
ls bin   # db  game  qc
```

The MariaDB client library is vendored, so no extra `-dev` packages are needed.

`server/m2dev-server-src-linux.patch` contains two Linux fixes (upstream mostly
targets FreeBSD/Windows):

- `fdwatch.cpp`: `select()` was called with `nfds = 0`. Windows ignores that
  argument, but Linux then watches no sockets and the server never accepts a
  connection.
- `AsyncSQL.cpp`: the locale query could run before the async MySQL connection
  was up. It now waits up to 10 s for the connection.

### 2.2 Runtime directory

```bash
git clone https://github.com/d1str4ught/m2dev-server ~/m2srv
cd ~/m2srv && git checkout ad30738
cp ~/m2dev-server-src/build/bin/{db,game,qc} share/bin/
cp share/bin/qc share/locale/english/quest/qc      # the repo ships a non-Linux qc
(cd share/locale/english/quest && python3 make.py)  # compile quests into object/
```

### 2.3 Database

The configs (`share/conf/db.txt`, `share/conf/game.txt`) expect a MySQL user
`mt2` with password `mt2@pw` on `127.0.0.1`. `sql/Readme.txt` in the runtime repo
says `metin2`/`password`, but the config files are what actually counts. Either
create the user below, or change both config files to your own credentials (do
that if the database is reachable from outside).

```bash
sudo mysql <<'SQL'
CREATE DATABASE account; CREATE DATABASE common; CREATE DATABASE player;
CREATE DATABASE log;     CREATE DATABASE hotbackup;
CREATE USER 'mt2'@'localhost' IDENTIFIED BY 'mt2@pw';
CREATE USER 'mt2'@'127.0.0.1' IDENTIFIED BY 'mt2@pw';
GRANT ALL PRIVILEGES ON *.* TO 'mt2'@'localhost';
GRANT ALL PRIVILEGES ON *.* TO 'mt2'@'127.0.0.1';
FLUSH PRIVILEGES;
SQL
cd ~/m2srv/sql
for db in account common player log; do sudo mysql $db < $db.sql; done
```

Set `sql_mode=NO_ENGINE_SUBSTITUTION` under `[mysqld]` (for example in
`/etc/mysql/mariadb.conf.d/99-metin2.cnf`) and restart MariaDB. Without it you
get "data truncated" errors.

The SQL dump comes with two accounts, `admin` and `test`, which share a
password hash. Change them before exposing the server. To add an account:

```sql
INSERT INTO account.account (login, password, social_id, status)
VALUES ('me', PASSWORD('secret'), '1234567', 'OK');
```

### 2.4 Configure, start, stop

```bash
cd ~/m2srv
python3 channels.py   # optional: edit channels/cores/maps in channels.py first
python3 install.py    # generates channels/*/CONFIG and symlinks share/bin binaries
python3 start.py 1    # db, auth and channel 1 (cores 1-3)
python3 stop.py
python3 clear.py      # wipe logs
```

Ports with the default `channels.py`:

| Process | Port |
|---|---|
| db (internal only) | 9000 |
| auth | 11000 |
| channel 1, core 1/2/3 | 11011 / 11012 / 11013 |
| P2P between cores (internal only) | 12000+ |

Clients connect to auth first, then to channel 1 core 1. When a character
stands on a map hosted by another core, the server tells the client to reconnect
to that core's IP and port. For example, the starting map 41 lives on core 3.
**Clients therefore need to reach all three core ports, not just 11011.**

### 2.5 The IP the server advertises

Each core announces its own address to clients. It auto-detects this from the
network interfaces and skips `10.*` and `192.168.*` addresses (see
`src/game/config.cpp`). The chosen value is printed as `PUBLIC_IP:` on the
core's stderr (`channels/channel1/coreN/stdout` when started via `start.py`). If it picks the wrong one, set it per core in
`channels/channel1/coreN/CONFIG`:

```
BIND_IP: 192.168.1.50
```

`install.py` regenerates CONFIG files, so add `BIND_IP` again after running it,
or add it to `generate_game_config` in `install.py`.

Check that the server is up:

```bash
tail -f channels/auth/syslog.log channels/channel1/core*/syslog.log
# successful login:  LOGIN_BY_KEY: test / ENTERGAME: Test ... map_index 41
```

## 3. Preparing the client data

```bash
sudo apt install libsodium23 rsync patch python3-pil
git clone https://github.com/d1str4ught/m2dev-client ~/m2dev-client
cd ~/m2dev-client && git checkout 98d2c1af
"$ANDROID/tools/stage_client_data.sh" ~/m2dev-client ~/m2stage en

mkdir -p ~/m2pylib && tar xzf Python-2.7.18.tgz -C /tmp
cp -r /tmp/Python-2.7.18/Lib/. ~/m2pylib/
rm -rf ~/m2pylib/{test,lib-tk,idlelib,lib2to3,ensurepip,bsddb,msilib,distutils,curses,pydoc_data,wsgiref.egg-info,site-packages,unittest} ~/m2pylib/plat-*
```

`stage_client_data.sh` does the following (it takes seconds and uses about 2 GB):

1. Merges every `assets/<pack>/` folder into one loose directory, in byte
   order with later packs winning, the same way the Windows client mounts packs.
   `assets/root/` goes on top, and `lib/`, `config/*.cfg` and `channel.inf` are
   copied over.
2. Re-encodes `locale/<lang>/item_proto` and `mob_proto`. m2dev encrypts them
   with XChaCha20, but this client reads the older layout
   (`tools/m2dev_proto_plain.py`).
3. Adds `m2compat.py`, which provides script functions that m2dev's UI expects
   but this engine does not export, and applies `tools/data-overlay/client-data.patch`:
   - `system.py` imports `m2compat`;
   - `intrologin.py` reads `loginInfo.py` instead of `.xml`;
   - `serverinfo.py` builds the login server list from `m2profile.py`, which the app
     writes at launch from the server list (section 4.3) and the build profile;
   - `game.py` and `uigameoption.py` load the touch HUD (`uimobilehud.py`) and
     add its "HUD: Desktop / Mobile" row to the game options window.
4. Builds the HUD textures into `mobile/` with `tools/data-overlay/make_hud_art.py`
   (Pillow). They are cut from the client's own `minimap.dds` and `public.dds`,
   so no extra art ships with the repo.

The engine lowercases only ASCII when it looks up paths, so keep file names
as they come. Do not lowercase Korean (CP949) names.

## 4. Building the Android client

### 4.1 Toolchain

- JDK 17, Gradle 8.1.1 (`/opt/gradle-8.1.1/bin/gradle`)
- Android SDK platform 33, build-tools 33, NDK `25.2.9519653`, CMake from the SDK

```bash
sdkmanager "platforms;android-33" "build-tools;33.0.2" "ndk;25.2.9519653" "cmake;3.22.1" "platform-tools"
echo "sdk.dir=$HOME/Android/Sdk" > "$ANDROID/local.properties"   # never commit this
```

### 4.2 Profiles

All server and data settings live in `$ANDROID/profiles/<name>.properties`.
You switch setups by choosing a profile, not by editing code.

| Key | Meaning |
|---|---|
| `m2.abi` | `arm64-v8a` for phones, `x86_64` for the emulator, or both comma-separated |
| `m2.serverHost` | IP/hostname of auth and of the cores |
| `m2.authPort` / `m2.channelPort` | 11000 / 11011 for a direct connection |
| `m2.gamePortOffset` | 0 for a direct connection. If a relay maps every port by a fixed offset, set the offset (e.g. 47111-11011 = 36100); core redirects are then rewritten to `serverHost:port+offset` |
| `m2.dataBundled` | `true` packs the data zip into the APK (about 1 GB; private sideloading only) |
| `m2.dataUrl` | URL of the data zip, or of a one-line text file containing that URL. Downloaded on first launch |
| `m2.dataVersion` | Bump to force devices to re-download/re-extract data |
| `m2.devUrl`, `m2.updateChannel` | Optional dev server for crash uploads and the in-app update check. Leave empty to disable |

Example for a server on your LAN:

```properties
# profiles/home.properties
m2.abi=arm64-v8a
m2.serverMode=remote
m2.serverHost=192.168.1.50
m2.authPort=11000
m2.channelPort=11011
m2.dataBundled=true
m2.dataVersion=2
m2.versionName=home
```

### 4.3 Server list

The login screen lists the servers in `$ANDROID/servers/servers.json`, in order:

```json
{ "version": 1, "servers": [
  { "name": "Offline", "embedded": true },
  { "name": "Istanbul", "host": "159.223.110.159", "authPort": 47100, "channelPort": 47111, "gamePortOffset": 36100 }
] }
```

- `embedded: true` is the server inside the app (127.0.0.1). It is only listed in
  builds that ship it (`m2.serverMode=embedded`).
- Names are `[A-Za-z0-9 _-]`, up to 24 characters; hosts are IPs or DNS names.
- `gamePortOffset` works as in the profile table above. Only one server can use it.
- The file is baked into the APK. At every launch the app also fetches the copy on
  `main` (`m2.serverListUrl`, HTTPS only, 3 s timeout) and keeps it if it parses and its
  `version` is not lower than the baked one. To add a server for installed apps, edit
  the file on `main` and bump `version`. Without network the last good copy, or the
  baked one, is used.
- A remote profile whose `serverHost` is not in the list shows it first as "Dev".

The current Istanbul entry is a placeholder: a tunnel to a development VM that is
only up while that VM runs.

### 4.4 Build

```bash
cd "$ANDROID"
M2_CLIENT_DATA=~/m2stage M2_PYLIB=~/m2pylib M2_BUNDLE_OUT=~/m2bundle \
  tools/make_bundle.sh home 1        # APK -> ~/m2bundle/metin2-home-1.apk
# or, without data packaging:
/opt/gradle-8.1.1/bin/gradle assembleDebug -Pm2profile=home
adb install -r ~/m2bundle/metin2-home-1.apk
```

`make_bundle.sh` zips the staged data together with `m2pylib` into
`m2data-<dataVersion>.zip`. It embeds the zip when `m2.dataBundled=true`;
otherwise it leaves the zip in `M2_BUNDLE_OUT` for you to host at `m2.dataUrl`.
The zip is reused if it already exists, so delete it (or bump
`m2.dataVersion`) after restaging data. Native symbols for crash traces are
saved to `M2_SYMS` (default `~/m2syms`).

A C++ change takes about 4-5 min per ABI to rebuild, or 1-2 min with
`ccache` on `PATH`.

### 4.5 Where the app keeps data

The data is extracted to `/sdcard/Android/data/com.metin2.client/files/`. The
same folder holds `syserr.txt`, `stderr.txt` and, after a native crash,
`crash.txt`. Read them with:

```bash
adb shell cat /sdcard/Android/data/com.metin2.client/files/syserr.txt
```

During development you can skip zipping and push the data once:
`adb push ~/m2stage/. /sdcard/Android/data/com.metin2.client/files/`.

### 4.6 Fully offline APK (embedded server)

The `bundled` profile puts the client data **and** the server inside one APK. On
launch the app starts db (9000), auth (11000) and one game core (11011) on 127.0.0.1, then
opens the game. No network, no PC and no adb pushes are needed after install.

1. Build the server pack once. It holds configs, map data, compiled quests and SQLite seed
   databases with the local `test` account:
   ```bash
   "r10dev.net OPENGL-ITJA/server/tools/make-server-pack.sh" ~/m2dev-server ~/m2dev-server-src ~/m2serverpack
   ```
   The server executables (`libm2db.so`, `libm2game.so`) are built by Gradle from the pinned
   server source for each ABI. MySQL is replaced by a SQLite shim in
   `r10dev.net OPENGL-ITJA/server/sqlite-mysql`.
2. Build and install:
   ```bash
   cd "$ANDROID"
   M2_SERVER_PACK=~/m2serverpack tools/make_bundle.sh bundled 1
   adb install -r ~/m2bundle/metin2-bundled-1.apk     # emulator: add --abi x86_64
   ```
3. On first launch it unpacks about 2 GB of data, shows "Starting local server...", then
   opens the game. Log in with the account from the seed databases.

Notes:
- The package is `com.metin2.client.offline` (labelled "Metin2"), kept from the first
  bundled builds so installed copies update in place.
- Characters live in the app's internal storage and survive restarts and updates. The
  server saves every 30 s, so a force-stop can lose up to about 30 s of progress.
- If a server process fails to start, the app shows which one, its exit status and the end of
  its log, plus a Retry button. Full logs are in `files/server/logs/` on external app storage.
- While a session is running, the app shows a "Game running. Tap to return." notification.
  That foreground service is what stops Android from killing the game in the background.

## 5. The browser client (wasm)

The same client also builds for the browser via Emscripten. `CLIENT` is the
C++ source; the web glue lives in `CLIENT/platform/web/` and `$ANDROID/../web/`.

### 5.1 Toolchain

```bash
git clone https://github.com/emscripten-core/emsdk ~/emsdk
~/emsdk/emsdk install latest && ~/emsdk/emsdk activate latest
source ~/emsdk/emsdk_env.sh          # emcc 6.0.11 was used
```

wasm builds of the Extern libs (Python 2.7, Crypto++, LZO) must exist in
`Extern/lib/web` + `Extern/include/Python2-web`; build them once with
`tools/wasm-deps/build.sh`.

### 5.2 Build and serve

```bash
# build -> build-web/metin2_web.{js,wasm,html}
"r10dev.net OPENGL-ITJA/web/build.sh"

# stage client data (same dir the Android flow uses, see section 3), then:
python3 "r10dev.net OPENGL-ITJA/web/manifest.py" --data-dir ~/m2data-web \
    --out ~/m2data-web/manifest.json

# static server + ws->tcp bridge + COOP/COEP headers in one:
python3 "r10dev.net OPENGL-ITJA/web/serve.py" --root ~/m2data-web --port 8081
# copy build-web/metin2_web.{js,wasm,html} into ~/m2data-web first, or point
# --root at a dir that has both the build output and the data
```

Open `http://<host>:8081/index.html`. First load downloads the boot tier
(~50 MB); `?m2_boot=1` waits for all ~51k manifest files before starting
(use when the login screen must be complete from frame 1).

Notes:

- The page needs COOP/COEP headers (pthreads/SharedArrayBuffer) — `serve.py`
  sets them; if you use another static server, set them yourself.
- Browsers can't open TCP, so the client wraps every connection in a
  WebSocket to `ws(s)://<page-origin>/ws?target=<host>:<port>`; `serve.py`
  bridges that to the real game server. To expose only the bridge,
  `web/ws_bridge.py` is the standalone version (`--allow` limits targets).
- `?m2_ws_bridge=ws://...` points the client at a different bridge;
  `?m2_game_host=...` overrides the connect host. Any `m2_*` query param
  becomes a process env var, and `debug.m2.*` maps to `GetDebugProperty`.
- `web/drive_headless.py <url> <secs> <shotdir> [actions.json]` drives
  headless Chrome over CDP — screenshots, console capture and a timed
  action list for scripted login tests.

## 6. Reaching the server from a phone

- **Same Wi-Fi:** set `m2.serverHost` to the server's LAN IP and make sure the
  cores advertise that IP (section 2.5). No offset is needed.
- **Over the internet:** forward TCP 11000 and 11011-11013 on your router to the
  server, set `m2.serverHost` to your public IP or a DNS name, and set
  `BIND_IP` on each core to that public IP.
- **Behind NAT without port forwarding:** use a TCP relay that maps each port
  by the same offset (e.g. `bore local 11011 --to bore.pub --port 47111` and
  likewise for 11000, 11012, 11013), then set `m2.gamePortOffset` to that
  offset. Free relays are slow and their ports get reused, so use one only for
  testing.

## 7. What must never be committed

Client data, data zips, APKs, `local.properties`, `.cxx/`, `build/`,
emulator images, database dumps with real accounts, and any credentials.

## 8. Troubleshooting

| Symptom | Cause |
|---|---|
| Disconnected right after the login screen | Server newer than `66068c1` (handshake `0xf8`) |
| Kicked back to server select after choosing a character | Client can't reach the core that hosts the character's map. Check `ENTERGAME`/redirect lines in syslog, `BIND_IP` and the reachable ports |
| Server accepts no connections | Linux patch not applied (`select` with `nfds = 0`) |
| `Data truncated` in db syserr | `sql_mode` not set (2.3) |
| Black screen / missing UI after login | Data not staged with `stage_client_data.sh` (protos/overlay missing). Check `syserr.txt` |
| `INSTALL_FAILED_INTERNAL_ERROR: Error deriving application ABI` | Uninstall the old app first, or build for the device's ABI only |

See `docs/PORTING_DIARY.md` for the engineering background on every fix.
