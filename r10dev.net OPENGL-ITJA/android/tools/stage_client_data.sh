#!/usr/bin/env bash
# Build the loose client-data directory this port reads, from an m2dev-client checkout.
#
# usage: tools/stage_client_data.sh <m2dev-client checkout> <output dir> [locale]
#
# Needs: python3, libsodium (apt install libsodium23), patch, rsync.
# Afterwards point M2_CLIENT_DATA at <output dir> for tools/make_bundle.sh.
set -euo pipefail

SRC=${1:?m2dev-client checkout}
OUT=${2:?output dir}
LOCALE=${3:-en}
HERE=$(cd "$(dirname "$0")" && pwd)
A="$SRC/assets"

[ -d "$A/root" ] || { echo "$SRC does not look like m2dev-client (no assets/root)"; exit 1; }
mkdir -p "$OUT/lib"

# Pack folders are merged in byte order; later packs override earlier ones,
# matching how the Windows client mounts its packs.
for pack in $(cd "$A" && LC_ALL=C ls -d */ | sed 's#/$##'); do
	[ "$pack" = root ] && continue
	rsync -a "$A/$pack/" "$OUT/"
done
rsync -a "$A/root/" "$OUT/"
rsync -a "$SRC/lib/" "$OUT/lib/"
rsync -a "$SRC/bgm/" "$OUT/bgm/"
cp "$SRC"/config/*.cfg "$SRC/channel.inf" "$OUT/"
printf '1252 %s\n' "$LOCALE" > "$OUT/loca.cfg"

for kind in item mob; do
	f="$OUT/locale/$LOCALE/${kind}_proto"
	python3 "$HERE/m2dev_proto_plain.py" "$A/locale/locale/$LOCALE/${kind}_proto" "$f.tmp" "$kind"
	mv "$f.tmp" "$f"
done

cp "$HERE/data-overlay/m2compat.py" "$HERE/data-overlay/uimobilehud.py" "$OUT/"
python3 "$HERE/data-overlay/make_hud_art.py" "$OUT"
patch -d "$OUT" -p1 --forward < "$HERE/data-overlay/client-data.patch"
echo "staged client data in $OUT"
