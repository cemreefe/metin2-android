#!/usr/bin/env bash
# Assemble a serveable webroot for the web build.
#
#   ./make-webroot.sh <clientdata-dir> <serverpack-dir> [out-dir]
#
#   <clientdata-dir>   the unpacked client data tree (index, pack/, lib/, ...)
#   <serverpack-dir>   output of server/tools/make-server-pack.sh
#                      (share/ + sqlite-seed/)
#   [out-dir]          webroot (default ./webroot)
#
# Produces: index.html (online shell), local.html (in-page server shell),
# client.m2pack, srv-share.m2pack, srv-sqlite.m2pack, packs.json,
# m2pack.js, srv_worker.js, metin2_web.{js,wasm}, m2db.{js,wasm},
# m2game.{js,wasm}. Serve with: python3 serve.py --root webroot --port 8081
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$HERE")"
CLIENT_DATA="${1:?clientdata dir required}"
SRV_PACK="${2:?server pack dir required}"
OUT="${3:-$HERE/webroot}"
CLIENT_BUILD="${M2_WEB_BUILD:-$ROOT/build-web}"
SRV_BUILD="${M2_SRV_WASM:-$ROOT/build-server-web}"

mkdir -p "$OUT"

cp "$CLIENT_BUILD/metin2_web.js" "$CLIENT_BUILD/metin2_web.wasm" "$OUT/"
cp "$CLIENT_BUILD/metin2_web.html" "$OUT/index.html"
M2V=$(cat "$CLIENT_BUILD/metin2_web.wasm" "$HERE"/*.js "$HERE/local.html" | md5sum | cut -c1-10)
sed -e "s/{{{ SCRIPT }}}/<script async src=\"metin2_web.js?v=$M2V\"><\/script>/" \
    -e "s/{{{ M2V }}}/$M2V/g" \
    "$HERE/local.html" > "$OUT/local.html"

cp "$HERE/m2pack.js" "$HERE/srv_worker.js" "$OUT/"
cp "$SRV_BUILD/bin/m2db.js" "$SRV_BUILD/bin/m2db.wasm" "$OUT/"
cp "$SRV_BUILD/bin/m2game.js" "$SRV_BUILD/bin/m2game.wasm" "$OUT/"

python3 "$HERE/pack.py" --data-dir "$CLIENT_DATA" --out "$OUT/client.m2pack" \
    --boot loose --boot lib/ --boot uiscript/ --boot locale/ --boot icon/ --boot fonts/ \
    --split-mb 150 \
    --manifest "$OUT/packs.json" --name client
python3 "$HERE/pack.py" --data-dir "$SRV_PACK/share" --out "$OUT/srv-share.m2pack" \
    --manifest "$OUT/packs.json" --name srv-share
python3 "$HERE/pack.py" --data-dir "$SRV_PACK/sqlite-seed" --out "$OUT/srv-sqlite.m2pack" \
    --manifest "$OUT/packs.json" --name srv-sqlite

echo "-> $OUT  (serve with: python3 $HERE/serve.py --root $OUT --port 8081)"
