#!/bin/bash
# Build a test bundle: APK for a profile + versioned client-data zip.
#   tools/make_bundle.sh <profile> [build-number]
# Env: M2_CLIENT_DATA (extracted client data dir), M2_PYLIB (Python 2.7 stdlib .py dir),
#      M2_BUNDLE_OUT (output dir served to devices), GRADLE (gradle binary),
#      M2_SERVER_PACK (make-server-pack.sh out dir; required for m2.serverMode=embedded
#      with m2.dataBundled=true, its server/ dir goes into the data zip).
set -euo pipefail
cd "$(dirname "$0")/.."
PROFILE=${1:?profile name, see profiles/}
BUILD=${2:-1}
DATA=${M2_CLIENT_DATA:-$HOME/m2stage}
PYLIB=${M2_PYLIB:-$HOME/m2pylib}
OUT=${M2_BUNDLE_OUT:-$HOME/m2bundle}
GRADLE=${GRADLE:-/opt/gradle-8.1.1/bin/gradle}
VERSION=$(sed -n 's/^m2.dataVersion=//p' "profiles/$PROFILE.properties")
NAME=$(sed -n 's/^m2.versionName=//p' "profiles/$PROFILE.properties")
BUNDLED=$(sed -n 's/^m2.dataBundled=//p' "profiles/$PROFILE.properties")
MODE=$(sed -n 's/^m2.serverMode=//p' "profiles/$PROFILE.properties")
mkdir -p "$OUT"
tools/make_icon.sh "$DATA"

ZIP="$OUT/m2data-$VERSION.zip"
SERVER_PACK=
if [ "$MODE" = "embedded" ] && [ "$BUNDLED" = "true" ]; then
  SERVER_PACK=${M2_SERVER_PACK:?set M2_SERVER_PACK to the make-server-pack.sh output dir}
  [ -f "$SERVER_PACK/server/share/conf/game.txt" ] || { echo "no server pack in $SERVER_PACK"; exit 1; }
  ZIP="$OUT/m2data-$VERSION-bundled.zip"
fi
# Installed copies only re-extract when m2.dataVersion changes, so new data under an old
# version would never reach them.
if [ -f "$ZIP" ] && [ -n "$(find "$DATA" ${SERVER_PACK:+"$SERVER_PACK/server"} -newer "$ZIP" -not -path '*/logs/*' -print -quit)" ]; then
  echo "client data changed since $ZIP was built: bump m2.dataVersion in profiles/$PROFILE.properties" >&2
  exit 1
fi
if [ ! -f "$ZIP" ]; then
  echo "building $ZIP"
  (cd "$DATA" && zip -q -1 -r "$ZIP.tmp" . -x 'syserr.txt' 'stderr.txt' 'm2profile.py*' '.m2data_version')
  OVERLAY=$(mktemp -d)
  ln -s "$PYLIB" "$OVERLAY/lib"
  (cd "$OVERLAY" && zip -q -1 -r "$ZIP.tmp" lib -x '*.pyc')
  rm -rf "$OVERLAY"
  if [ -n "$SERVER_PACK" ]; then
    (cd "$SERVER_PACK" && zip -q -1 -r "$ZIP.tmp" server -x 'server/logs/*')
  fi
  mv "$ZIP.tmp" "$ZIP"
fi

EXTRA=()
if [ "$BUNDLED" = "true" ]; then
  ASSETS="$OUT/assets-$VERSION"
  mkdir -p "$ASSETS"
  ln -f "$ZIP" "$ASSETS/m2data.zip"
  EXTRA=(-Pm2dataAssets="$ASSETS")
fi
# Incremental packaging leaves the old asset bytes in place when the data zip changes,
# doubling the APK; always package from scratch.
rm -f app/build/outputs/apk/debug/app-debug.apk
"$GRADLE" assembleDebug -Pm2profile="$PROFILE" -Pm2build="$BUILD" "${EXTRA[@]}" -q
cp app/build/outputs/apk/debug/app-debug.apk "$OUT/metin2-$NAME-$BUILD.apk"
SYMS=${M2_SYMS:-$HOME/m2syms}/$NAME-$BUILD
mkdir -p "$SYMS"
for abi in app/build/intermediates/merged_native_libs/debug/out/lib/*/; do
  cp "$abi/libmetin2_mobile.so" "$SYMS/libmetin2_mobile-$(basename "$abi").so"
done
CHANNEL=$(sed -n 's/^m2.updateChannel=//p' "profiles/$PROFILE.properties")
DEVURL=$(sed -n 's/^m2.devUrl=//p' "profiles/$PROFILE.properties")
if [ -n "$CHANNEL" ] && [ -n "$DEVURL" ] && [ "$BUNDLED" != "true" ]; then
  echo "$BUILD $DEVURL/metin2-$NAME-$BUILD.apk" > "$OUT/latest-$CHANNEL.txt"
fi
echo "$OUT/metin2-$NAME-$BUILD.apk"
echo "$ZIP"
