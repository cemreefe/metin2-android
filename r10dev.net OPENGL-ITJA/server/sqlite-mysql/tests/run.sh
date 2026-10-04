#!/usr/bin/env bash
# Builds and runs the shim's host tests with the system compiler.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SHIM="$(dirname "$HERE")"
OUT="${TMPDIR:-/tmp}/m2-sqlite-mysql-tests"
mkdir -p "$OUT"

if [ ! -f "$OUT/sqlite3.o" ]; then
	cc -O1 -I"$SHIM/sqlite3" -c "$SHIM/sqlite3/sqlite3.c" -o "$OUT/sqlite3.o"
fi

c++ -std=c++14 -O0 -g -I"$SHIM/include" -I"$SHIM/sqlite3" \
	-o "$OUT/test_enum" "$HERE/test_enum.cpp" "$SHIM/mysql_sqlite.cpp" "$OUT/sqlite3.o" \
	-lpthread -ldl

rm -rf "$OUT/db"
"$OUT/test_enum" "$OUT/db"
