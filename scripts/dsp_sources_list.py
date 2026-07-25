#!/usr/bin/env python3
"""Extract C/C++ SOURCE= entries from a Visual C++ 6 .dsp project file."""
from __future__ import annotations

import re
import sys


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <project.dsp>", file=sys.stderr)
        return 2

    dsp_path = sys.argv[1]
    source_re = re.compile(r"^SOURCE=(.+)$")
    seen = set()

    with open(dsp_path, "r", encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            line = raw.strip()
            match = source_re.match(line)
            if not match:
                continue
            path = match.group(1).strip().strip('"').replace("\\", "/")
            if path.startswith("./"):
                path = path[2:]
            lower = path.lower()
            if not lower.endswith((".c", ".cc", ".cpp", ".cxx")):
                continue
            if path in seen:
                continue
            seen.add(path)
            print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
