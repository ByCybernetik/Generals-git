#!/usr/bin/env python3
"""Extract SOURCE= compile units from VC6 .dsp into Meson files('...') snippets."""
from __future__ import annotations

import re
import sys
from pathlib import Path

SOURCE_RE = re.compile(r"^SOURCE=\.\\(.+)$", re.IGNORECASE)
EXTS = {".cpp", ".c", ".cc", ".cxx"}


def parse_dsp(dsp: Path, dsp_dir: Path) -> list[str]:
    out: list[str] = []
    seen: set[str] = set()
    for line in dsp.read_text(encoding="latin-1", errors="replace").splitlines():
        m = SOURCE_RE.match(line.strip())
        if not m:
            continue
        rel = m.group(1).replace("\\", "/")
        if Path(rel).suffix.lower() not in EXTS:
            continue
        full = (dsp_dir / rel).resolve()
        if not full.is_file():
            print(f"warn: missing {rel} ({dsp.name})", file=sys.stderr)
            continue
        if rel in seen:
            continue
        seen.add(rel)
        out.append(rel)
    out.sort(key=str.lower)
    return out


def write_meson_snippet(path: Path, rel_paths: list[str], var_name: str) -> None:
  lines = [f"{var_name} = files("]
  for p in rel_paths:
      lines.append(f"  '{p}',")
  lines.append(")")
  path.write_text("\n".join(lines) + "\n", encoding="utf-8")
  print(f"wrote {path} ({len(rel_paths)} sources)")


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    jobs = [
        (
            root / "Code/Libraries/Source/Compression/Compression.dsp",
            root / "Code/Libraries/Source/Compression/compression_sources.meson",
            "compression_sources",
        ),
        (
            root / "Code/GameEngine/GameEngine.dsp",
            root / "Code/GameEngine/gameengine_sources.meson",
            "gameengine_sources",
        ),
        (
            root / "Code/GameEngineDevice/GameEngineDevice.dsp",
            root / "Code/GameEngineDevice/gameenginedevice_sources.meson",
            "gameenginedevice_sources",
        ),
    ]
    for dsp, out, var in jobs:
        if not dsp.is_file():
            print(f"error: missing {dsp}", file=sys.stderr)
            return 1
        rels = parse_dsp(dsp, dsp.parent)
        write_meson_snippet(out, rels, var)
    return 0


if __name__ == "__main__":
    sys.exit(main())
