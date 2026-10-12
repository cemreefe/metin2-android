# m2 server — self-hosted (Docker + Cloudflare Tunnel)

One container runs db + auth + channel1 + the ws→tcp bridge the web client
needs. SQLite databases live on a Docker volume and survive restarts.
Cloudflare Tunnel exposes the bridge publicly — no port forwarding, your
home IP stays hidden, TLS is handled by Cloudflare.

## Assembling the build context

The Dockerfile needs four things next to it:

```
hosted/
  src/          patched m2dev-server-src checkout (m2dev-server-src-web.patch
                applied; web/m2lp* under __EMSCRIPTEN__ only, so it builds
                natively)
  srvcmake/     this repo's server/ dir (CMake wrapper + sqlite-mysql shim)
  share/        runtime data from tools/make-server-pack.sh <out>/server/share
  sqlite-seed/  from the same pack output
```

Build the data once on a dev machine:

```bash
tools/make-server-pack.sh <m2dev-server checkout> <m2dev-server-src checkout> <out>
cp -r <m2dev-server-src> src && rm -rf src/.git
cp -r .. srvcmake            # this repo's server/ dir
cp -r <out>/server/share <out>/server/sqlite-seed .
```

## Setup (once)

1. Install Docker on the host.
2. Cloudflare Zero Trust dashboard → Networks → Tunnels → Add a tunnel
   → "cloudflared". Copy the tunnel token (starts `eyJ...`).
3. `cp .env.example .env` and paste the token.
4. In the tunnel's Public Hostnames, add the realm hostname
   (e.g. `srv.m2.dutl.uk`) → service `http://m2:8000`.
5. `docker compose up -d` — first run builds the server from source inside
   Docker; works on amd64 and arm64.

## Playing

`https://m2.dutl.uk/?realm=Dutluk` — the gate screen also links it.
The list shows one server + CH1 and the real login page (no hidden account).

## Accounts & GM

```bash
docker compose exec m2 /app/adduser.sh <login> <password>
docker compose exec m2 /app/gm.sh <login> <character>       # IMPLEMENTOR
```

GM applies on the character's next login. The seed ships `test`/`test123` —
change or delete it:
`docker compose exec m2 sqlite3 /data/sqlite/account.sqlite3 "DELETE FROM account WHERE login='test'"`

## Operating

- logs:   `docker compose logs -f m2` (per-node: `docker compose exec m2 tail -f /srv/chan1/log/out.log`)
- stop:   `docker compose down` (data stays in the `m2data` volume)
- wipe:   `docker compose down -v` (deletes all accounts/saves)
- maps:   edit `M2_MAP_ALLOW` in `.env` — indices in `share/locale/english/map/index`
- backup: `docker run --rm -v hosted_m2data:/data -v $PWD:/b alpine tar czf /b/m2backup-$(date +%F).tgz /data`

The bridge only accepts targets `127.0.0.1:11000` / `127.0.0.1:11011`
(`M2_ALLOWED_TARGETS`), so it can't be abused as a TCP relay.
