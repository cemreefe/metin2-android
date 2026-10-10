#!/usr/bin/env bash
# Create a player account. Run inside the m2 container:
#   docker compose exec m2 /app/adduser.sh <login> <password>
set -eu
[ $# -ge 2 ] || { echo "usage: adduser.sh <login> <password>"; exit 1; }
L="$1"; P="$2"
DB="${M2_DATA:-/data}/sqlite/account.sqlite3"
case "$L$P" in *"'"*) echo "no quotes in login/password"; exit 1;; esac
H=$(python3 - "$P" <<'PY'
import hashlib, sys
pw = sys.argv[1]
print("*" + hashlib.sha1(hashlib.sha1(pw.encode()).digest()).hexdigest().upper())
PY
)
sqlite3 "$DB" "INSERT INTO account (login,password,status,create_time,empire) VALUES ('$L','$H','OK',datetime('now'),0);"
echo "created account '$L'"
