#!/bin/bash
# Build a test bundle: APK for a profile + versioned client-data zip.
#   tools/make_bundle.sh <profile> [build-number]
# Env: M2_CLIENT_DATA (extracted client data dir), M2_PYLIB (Python 2.7 stdlib .py dir),
#      M2_BUNDLE_OUT (output dir served to devices), GRADLE (gradle binary).
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
mkdir -p "$OUT"

ZIP="$OUT/m2data-$VERSION.zip"
if [ ! -f "$ZIP" ]; then
  echo "building $ZIP"
  (cd "$DATA" && zip -q -1 -r "$ZIP.tmp" . -x 'syserr.txt' 'stderr.txt' 'm2profile.py*' '.m2data_version')
  OVERLAY=$(mktemp -d)
  ln -s "$PYLIB" "$OVERLAY/lib"
  (cd "$OVERLAY" && zip -q -1 -r "$ZIP.tmp" lib -x '*.pyc')
  rm -rf "$OVERLAY"
  mv "$ZIP.tmp" "$ZIP"
fi

EXTRA=()
if [ "$BUNDLED" = "true" ]; then
  ASSETS="$OUT/assets-$VERSION"
  mkdir -p "$ASSETS"
  ln -f "$ZIP" "$ASSETS/m2data.zip"
  EXTRA=(-Pm2dataAssets="$ASSETS")
fi
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
