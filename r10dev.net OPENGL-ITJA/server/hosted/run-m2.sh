#!/usr/bin/env bash
# Supervisor for the m2 server nodes: db + auth + channel1 + ws bridge,
# all on 127.0.0.1 inside this container. Mirrors the node layout the
# Android/web embedded server uses: per-node cwd with symlinks into
# /app/share, a CONFIG file, and M2_SQLITE_DIR pointing at the volume.
set -u
DATA="${M2_DATA:-/data}"
mkdir -p "$DATA/sqlite"
if [ ! -f "$DATA/sqlite/account.sqlite3" ]; then
  echo "[m2] seeding sqlite databases into $DATA/sqlite"
  cp /app/seed/*.sqlite3 "$DATA/sqlite/"
fi
export M2_SQLITE_DIR="$DATA/sqlite"

mknode() {
  mkdir -p "/srv/$1/log"
  for d in conf data locale mark package; do ln -sfn "/app/share/$d" "/srv/$1/$d"; done
}

run() { # run <node> <argv...>
  local node="$1"; shift
  mkdir -p "/srv/$node/log"
  ( while :; do
      cd "/srv/$node"
      "$@" >>"/srv/$node/log/out.log" 2>&1
      echo "[m2] $node exited ($?), restarting in 2s" >>"/srv/$node/log/out.log"
      sleep 2
    done ) &
}

mknode db
run db /app/bin/db

# auth/chan open a connection to the db worker at boot and do not retry on
# failure — wait for :9000 to accept connections before spawning them.
for i in $(seq 1 60); do
  (exec 3<>/dev/tcp/127.0.0.1/9000 && exec 3>&- && exec 3<&-) 2>/dev/null && break
  sleep 1
done

mknode auth
cat > /srv/auth/CONFIG <<'EOF'
HOSTNAME: auth
CHANNEL: 1
PORT: 11000
P2P_PORT: 12100
AUTH_SERVER: master
EOF
run auth /app/bin/game -I 127.0.0.1

mknode chan1
cat > /srv/chan1/CONFIG <<EOF
HOSTNAME: channel1_1
CHANNEL: 1
PORT: 11011
P2P_PORT: 12111
MAP_ALLOW: ${M2_MAP_ALLOW:-1 21 41}
EOF
run chan1 /app/bin/game -I 127.0.0.1

run bridge python3 /app/wsbridge.py --port 8000

trap 'kill $(jobs -p) 2>/dev/null' TERM INT
echo "[m2] db:9000 auth:11000 chan1:11011 bridge:8000 (sqlite: $DATA/sqlite)"
wait
