#!/usr/bin/env bash
# Pushes <out>/server from make-server-pack.sh to the device, next to the client data.
#   push-server-pack.sh <out dir>   (ADB / ANDROID_SERIAL honoured)
set -euo pipefail
ADB=${ADB:-adb}
DEST=/storage/emulated/0/Android/data/com.metin2.client/files
"$ADB" shell mkdir -p "$DEST"
"$ADB" shell rm -rf "$DEST/server"
"$ADB" push "$1/server" "$DEST/"
# Under `adb root` pushed files get root ownership and the wrong SELinux label
# (storage_file); the app can't read them until both are restored.
LOWER=/data/media/0/Android/data/com.metin2.client
"$ADB" shell "[ \$(id -u) = 0 ] && chown -R \$(stat -c %u:%g $LOWER) $LOWER/files/server && restorecon -R $LOWER" || true
"$ADB" shell ls "$DEST/server" "$DEST/server/sqlite-seed"
