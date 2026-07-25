#!/usr/bin/env python3
"""Print compile-unit paths (one per line) from a VC6 .dsp, relative to the .dsp directory."""
from __future__ import annotations

import re
import sys
from pathlib import Path

SOURCE_RE = re.compile(r"^SOURCE=\.\\(.+)$", re.IGNORECASE)
EXTS = {".cpp", ".c", ".cc", ".cxx"}


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <project.dsp>", file=sys.stderr)
        return 2
    dsp = Path(sys.argv[1]).resolve()
    dsp_dir = dsp.parent
    seen: set[str] = set()
    for line in dsp.read_text(encoding="latin-1", errors="replace").splitlines():
        m = SOURCE_RE.match(line.strip())
        if not m:
            continue
        rel = m.group(1).replace("\\", "/")
        if Path(rel).suffix.lower() not in EXTS:
            continue
        if rel in seen:
            continue
        seen.add(rel)
        if not (dsp_dir / rel).is_file():
            continue
        print(rel)
    return 0


if __name__ == "__main__":
    sys.exit(main())
