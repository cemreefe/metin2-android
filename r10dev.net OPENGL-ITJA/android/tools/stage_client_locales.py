#!/usr/bin/env python3
"""Prepare every selectable locale for the legacy client data readers."""
from pathlib import Path
import sys

from m2dev_proto_plain import convert


CODE_PAGES = {
    "tr": 1254, "ru": 1251, "ae": 1256, "gr": 1253,
    "pl": 1250, "cz": 1250, "hu": 1250, "ro": 1250,
}


def stage_locales(output, selected):
    output = Path(output)
    for locale in sorted((output / "locale").iterdir()):
        if not locale.is_dir():
            continue
        for kind in ("item", "mob"):
            proto = locale / (kind + "_proto")
            if proto.is_file():
                temporary = proto.with_suffix(".tmp")
                encoding = "cp1254" if locale.name == "tr" else None
                convert(proto, temporary, kind, encoding)
                temporary.replace(proto)

        # Turkish assets are UTF-8; the client consumes Windows Turkish bytes.
        if locale.name == "tr":
            for path in locale.rglob("*.txt"):
                raw = path.read_bytes()
                try:
                    text = raw.decode("utf-8-sig")
                except UnicodeDecodeError:
                    continue
                path.write_bytes(text.encode("cp1254"))

    code_page = CODE_PAGES.get(selected, 1252)
    (output / "loca.cfg").write_text("%d %s\n" % (code_page, selected), encoding="ascii")


if __name__ == "__main__":
    stage_locales(*sys.argv[1:3])
