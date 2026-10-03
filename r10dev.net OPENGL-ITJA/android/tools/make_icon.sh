#!/bin/bash
# Generate the launcher icon from the client's own login logo (ymir work/ui/intrologin.dds).
# Output goes to app/build/generated-icon (not committed; the art belongs to the game data).
#   tools/make_icon.sh [client-data-dir]   (needs ImageMagick)
set -euo pipefail
cd "$(dirname "$0")/.."
DATA=${1:-${M2_CLIENT_DATA:-$HOME/m2stage}}
SRC="$DATA/ymir work/ui/intrologin.dds"
[ -f "$SRC" ] || { echo "no $SRC; skipping icon"; exit 0; }
OUT=app/build/generated-icon
TMP=$(mktemp -d)
convert "$SRC" -crop 429x170+0+0 +repage "$TMP/metin.png"
convert "$SRC" -crop 142x170+0+170 +repage "$TMP/two.png"
convert -size 512x512 radial-gradient:'#5a1a10'-'#120604' \
  \( "$TMP/metin.png" -resize 470x \) -gravity north -geometry +0+70 -composite \
  \( "$TMP/two.png" -resize x260 \) -gravity north -geometry +0+215 -composite \
  \( -size 512x512 xc:black -fill white -draw "roundrectangle 0,0 511,511 96,96" \) \
  -alpha off -compose CopyOpacity -composite -depth 8 "$TMP/icon.png"
for d in mdpi:48 hdpi:72 xhdpi:96 xxhdpi:144 xxxhdpi:192; do
  mkdir -p "$OUT/mipmap-${d%%:*}"
  convert "$TMP/icon.png" -resize "${d##*:}x${d##*:}" "$OUT/mipmap-${d%%:*}/ic_launcher.png"
done
rm -rf "$TMP"
echo "icon -> $OUT"
