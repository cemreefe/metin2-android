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

# TPS Carbine (vnum 5301): the server reads protos from conf/item_proto.txt,
# not the sqlite item_proto table — clone the battle bow's row here so the
# seeded item isn't dropped as an unknown vnum at load. The bow's antiflags
# (ANTI_MUSA|ANTI_SURA|ANTI_MUDANG) would ban every non-ninja job from
# equipping it, so they are stripped. Always regenerated: a stale row from
# an older run carries the ban.
sed -i '/^5301\t/d' "$OUT/server/share/conf/item_proto.txt"
# upstream file has no trailing newline; append would glue onto its last row
printf '\n' >> "$OUT/server/share/conf/item_proto.txt"
grep -P "^2030\t" "$OUT/server/share/conf/item_proto.txt" \
    | sed -e 's/^2030/5301/' -e 's/Battle Bow+0/TPS Carbine/' \
          -e 's/\t2031\t/\t0\t/' \
          -e 's/ANTI_MUSA | ANTI_SURA | ANTI_MUDANG/NONE/' \
    >> "$OUT/server/share/conf/item_proto.txt"

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
if [ -f "$HERE/tps-seed.sql" ]; then
    python3 - "$OUT/server/sqlite-seed" "$HERE/tps-seed.sql" "${ACCOUNT%%:*}" <<'PY'
import sqlite3, sys
seed_dir, sql_path, login = sys.argv[1], sys.argv[2], sys.argv[3]
con = sqlite3.connect(seed_dir + '/player.sqlite3')
# Enum-conversion triggers installed by mysql2sqlite.py reference this UDF;
# our seed rows already carry text values, so a passthrough is enough.
con.create_function("m2_enum_text", 3, lambda v, _enums, _base: v)
con.executescript(open(sql_path).read())
# Starter carbine for the test account's first character. account is a
# separate sqlite database, so attach it for the lookup.
con.execute("ATTACH DATABASE ? AS acct", (seed_dir + '/account.sqlite3',))
row = con.execute(
    "SELECT p.id FROM player p JOIN acct.account a ON a.id = p.account_id"
    " WHERE a.login = ? ORDER BY p.id LIMIT 1", (login,)).fetchone()
if row:
    # EQUIPMENT-window positions are wear-cell indices (WEAR_* enum):
    # 0=WEAR_BODY ... 4=WEAR_WEAPON. Anything out of range (or a weapon in a
    # non-weapon cell) is silently dropped at load -> pos must be 4.
    # Replace whatever starter weapon sits there so the char spawns holding
    # the carbine and nothing else is drawn in the hands.
    con.execute("DELETE FROM item WHERE owner_id = ? AND window = 'EQUIPMENT' AND pos = 4", (row[0],))
    con.execute(
        "INSERT OR REPLACE INTO item (id, owner_id, window, pos, count, vnum)"
        " VALUES (20000999, ?, 'EQUIPMENT', 4, 1, 5301)", (row[0],))
con.commit()
PY
fi

du -sh "$OUT/server"
