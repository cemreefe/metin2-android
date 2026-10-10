#!/usr/bin/env bash
# Grant GM authority to a character. Run inside the m2 container:
#   docker compose exec m2 /app/gm.sh <account-login> <character-name> [authority]
# Authority is one of IMPLEMENTOR, HIGH_WIZARD, GOD, LOW_WIZARD, PLAYER.
set -eu
[ $# -ge 2 ] || { echo "usage: gm.sh <login> <character> [authority]"; exit 1; }
L="$1"; N="$2"; A="${3:-IMPLEMENTOR}"
DB="${M2_DATA:-/data}/sqlite/common.sqlite3"
case "$L$N" in *"'"*) echo "no quotes in names"; exit 1;; esac
sqlite3 "$DB" "INSERT INTO gmlist (mAccount,mName,mAuthority) VALUES ('$L','$N','$A');"
echo "granted $A to character '$N' on account '$L' — takes effect next login"
