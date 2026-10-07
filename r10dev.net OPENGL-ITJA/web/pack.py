#!/usr/bin/env python3
"""Pack a directory into .m2pack bundles for m2pack.js.

Replaces the manifest.json per-file fetch: a few compressed downloads,
IndexedDB-cached in the browser, streamed into MEMFS.

    python3 pack.py --data-dir /path/to/clientdata --out client.m2pack \
        [--boot loose --boot lib/ --boot uiscript/ --boot locale/] \
        [--split-mb 150] [--manifest packs.json --name client]

--boot marks boot-tier files, written to a separate <base>-boot.m2pack so
the page can start main() after only that download. 'loose' matches files
with no '/' in the path; 'x/' matches paths under that prefix.

--split-mb N splits the non-boot files into <base>-p1..pN archives of at
most N raw megabytes each — keeps browser fetch buffers small and gives
per-part IndexedDB caching (a failed download retries one part, not 1GB).

--manifest writes/updates packs.json:
  {name: {boot: {url,ver}|absent, parts: [{url,ver}...]}}
ver is each part's sha256 — the page compares it against IndexedDB.
"""
import argparse
import gzip
import hashlib
import json
import os
import struct


def is_boot(rel, rules):
    for r in rules:
        if r == 'loose' and '/' not in rel:
            return True
        if r != 'loose' and rel.startswith(r):
            return True
    return False


def write_pack(files, out_path):
    """files: [(rel, full)] -> M2PK archive, gzip'd. Returns sha256[:16] of raw."""
    out = bytearray()
    out += struct.pack("<4sII", b"M2PK", 0, len(files))
    for rel, full in files:
        p = rel.encode()
        out += struct.pack("<I", len(p)) + p
        out += struct.pack("<Q", os.path.getsize(full))
        with open(full, "rb") as f:
            while True:
                chunk = f.read(1 << 20)
                if not chunk:
                    break
                out += chunk
    raw = bytes(out)
    with open(out_path, "wb") as f:
        f.write(gzip.compress(raw, 9))
    return hashlib.sha256(raw).hexdigest()[:16], len(raw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-dir", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--boot", action="append", default=[])
    ap.add_argument("--split-mb", type=int, default=0)
    ap.add_argument("--manifest", default=None)
    ap.add_argument("--name", default=None)
    args = ap.parse_args()

    boot, rest = [], []
    for root, _dirs, files in os.walk(args.data_dir):
        for name in sorted(files):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, args.data_dir).replace(os.sep, "/")
            (boot if is_boot(rel, args.boot) else rest).append((rel, full))

    base = args.out
    if base.endswith(".m2pack"):
        base = base[:-7]

    entry = {"parts": []}

    if boot:
        path = base + "-boot.m2pack"
        sha, rawsz = write_pack(boot, path)
        entry["boot"] = {"url": os.path.basename(path), "ver": sha}
        print(f"{len(boot)} boot files -> {path} "
              f"[{rawsz/1e6:.0f}MB raw, {os.path.getsize(path)/1e6:.0f}MB gz] ver={sha}")

    # chunk rest into split-mb-sized archives
    parts, cur, cursz = [], [], 0
    limit = args.split_mb * 1e6 if args.split_mb else 0
    for rel, full in rest:
        sz = os.path.getsize(full)
        if limit and cur and cursz + sz > limit:
            parts.append(cur)
            cur, cursz = [], 0
        cur.append((rel, full))
        cursz += sz
    if cur:
        parts.append(cur)
    if not parts:
        parts = [[]]

    for i, part in enumerate(parts):
        path = f"{base}-p{i+1}.m2pack" if len(parts) > 1 or boot else args.out
        if not part:
            continue
        sha, rawsz = write_pack(part, path)
        entry["parts"].append({"url": os.path.basename(path), "ver": sha})
        print(f"{len(part)} files -> {path} "
              f"[{rawsz/1e6:.0f}MB raw, {os.path.getsize(path)/1e6:.0f}MB gz] ver={sha}")

    if args.manifest:
        packs = {}
        if os.path.exists(args.manifest):
            with open(args.manifest) as f:
                packs = json.load(f)
        packs[args.name or os.path.basename(args.out)] = entry
        with open(args.manifest, "w") as f:
            json.dump(packs, f, indent=1)


if __name__ == "__main__":
    main()
