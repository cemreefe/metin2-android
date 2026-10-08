#!/usr/bin/env python3
"""Generate manifest.json for the data prefetch.

Scans a client data directory (system.py, pack/*.eix+*.epk, index, font.ttf,
locale.cfg, ...) and emits the flat file list the shell downloads into
MEMFS before main() runs. Paths are repo-relative URL paths — serve the
data dir from the same origin.

    python3 manifest.py --data-dir /path/to/clientdata --out manifest.json \
        [--strip pack/]

--strip prefixes each path in the manifest (e.g. if your files live in a
'pack/' subdir on the server but the engine opens 'metin2_map_a1.eix').
"""
import argparse
import json
import os


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data-dir", required=True)
    ap.add_argument("--out", default="manifest.json")
    ap.add_argument("--strip", default="")
    args = ap.parse_args()

    entries = []
    for root, _dirs, files in os.walk(args.data_dir):
        for name in sorted(files):
            full = os.path.join(root, name)
            rel = os.path.relpath(full, args.data_dir).replace(os.sep, "/")
            if args.strip and rel.startswith(args.strip):
                rel = rel[len(args.strip):]
            entries.append({"path": rel, "size": os.path.getsize(full)})

    with open(args.out, "w") as f:
        json.dump(entries, f)
    print(f"{len(entries)} files -> {args.out}")


if __name__ == "__main__":
    main()
