#!/usr/bin/env bash
# Builds the external "server data" pack for the embedded (offline) profile.
#
#   make-server-pack.sh <m2dev-server checkout> <m2dev-server-src checkout> <out dir> [LOGIN:PASSWORD]
#
# Output layout (push <out>/server next to the client data):
#   server/share/{conf,data,locale,mark,package}   runtime data, quests compiled with a host qc
#   server/sqlite-seed/*.sqlite3                   schema + upstream rows + local test account
set -euo pipefail

RUNTIME=$(cd "$1" && pwd)
SRC=$(cd "$2" && pwd)
OUT=$(mkdir -p "$3" && cd "$3" && pwd)
ACCOUNT=${4:-test:test123}
HERE=$(cd "$(dirname "$0")" && pwd)
SERVER_DIR=$(dirname "$HERE")

CMAKE=${CMAKE:-cmake}
command -v "$CMAKE" >/dev/null || CMAKE=$(ls -d "${ANDROID_HOME:-$HOME/Android/Sdk}"/cmake/*/bin/cmake | tail -1)
NINJA=$(dirname "$CMAKE")/ninja
[ -x "$NINJA" ] || NINJA=ninja

echo "== host qc"
HOST_BUILD="$OUT/.host-build"
"$CMAKE" -S "$SERVER_DIR" -B "$HOST_BUILD" -G Ninja -DCMAKE_MAKE_PROGRAM="$NINJA" \
    -DM2_SERVER_SRC="$SRC" -DCMAKE_BUILD_TYPE=Release >/dev/null
"$CMAKE" --build "$HOST_BUILD" --target qc

echo "== share"
rm -rf "$OUT/server"
mkdir -p "$OUT/server/share/package"
cp -r "$RUNTIME/share/conf" "$RUNTIME/share/data" "$RUNTIME/share/locale" "$RUNTIME/share/mark" "$OUT/server/share/"

# Shorter save/flush cycles: Android may kill the app without a clean shutdown.
cat >> "$OUT/server/share/conf/db.txt" <<'CONF'

PLAYER_CACHE_FLUSH_SECONDS = 30
ITEM_CACHE_FLUSH_SECONDS = 30
CONF
sed -i 's/^SAVE_EVENT_SECOND_CYCLE:.*/SAVE_EVENT_SECOND_CYCLE: 30/' "$OUT/server/share/conf/game.txt"

echo "== quests"
for Q in "$OUT"/server/share/locale/*/quest; do
    rm -f "$Q/qc" "$Q/qc.exe"
    cp "$HOST_BUILD/bin/qc" "$Q/qc"
    chmod +x "$Q/qc"
    (cd "$Q" && python3 make.py >"$OUT/qc.log" 2>&1) || { tail -20 "$OUT/qc.log"; exit 1; }
    rm -f "$Q/qc"
    echo "$(find "$Q/object" -type f | wc -l) quest objects in $Q/object"
done

echo "== sqlite seed"
python3 "$HERE/mysql2sqlite.py" "$RUNTIME/sql" "$OUT/server/sqlite-seed" --test-account "$ACCOUNT"

du -sh "$OUT/server"
