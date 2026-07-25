#!/usr/bin/env python3
"""Print GameSpy .c paths for Linux (excludes Win32/macOS platform sources)."""
from __future__ import annotations

import sys
from pathlib import Path

SUBDIRS = (
    "common",
    "gp",
    "peer",
    "ghttp",
    "gstats",
    "gt2",
    "natneg",
    "qr",
    "qr2",
    "pinger",
    "serverbrowsing",
    "gcdkey",
    "chat",
)

SKIP_DIRS = {"macosx", "win32", "psp", "ps2", "ps3", "nds", "nintendo", "xbox", "nitro"}


def main() -> int:
    root = Path(__file__).resolve().parents[1] / "Code/Libraries/Source/GameSpy"
    vendor_src = root / "vendor" / "src"
    if not vendor_src.is_dir():
        return 0
    for sub in SUBDIRS:
        d = vendor_src / sub
        if not d.is_dir():
            continue
        for path in sorted(d.rglob("*.c")):
            rel_parts = path.relative_to(d).parts
            if rel_parts and rel_parts[0] in SKIP_DIRS:
                continue
            if "test" in rel_parts or "sample" in rel_parts:
                continue
            print(path.relative_to(root).as_posix())
    return 0


if __name__ == "__main__":
    sys.exit(main())
