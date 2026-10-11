import argparse
import re
from pathlib import Path


def render(template, version):
    if not re.fullmatch(r"[A-Za-z0-9_-]+", version):
        raise ValueError("invalid build version")
    if template.count("{{{ SCRIPT }}}") != 1:
        raise ValueError("expected exactly one engine script placeholder")
    html = template.replace(
        "{{{ SCRIPT }}}",
        '<script async src="metin2_web.js?v=' + version + '"></script>',
    ).replace("{{{ M2V }}}", version)
    validate(html)
    return html


def validate(html):
    if "{{{" in html or "}}}" in html:
        raise ValueError("unexpanded deployment template")
    if not re.search(r'<script\s+async\s+src="metin2_web\.js\?v=[\w-]+"></script>', html):
        raise ValueError("missing engine script")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("template", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("version")
    args = parser.parse_args()
    args.output.write_text(render(args.template.read_text(), args.version))
