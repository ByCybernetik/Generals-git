#!/usr/bin/env bash
# Populate gitignored third-party sources for Phase 3 (Compression, GameSpy SDK).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ZH_DEPS="${ZERO_HOUR_DEPS:-/home/cybernetik/Other/Sources/CPP/CnC_Generals_Zero_Hour-develop/build/_deps}"

LZHL_SRC="$ZH_DEPS/liblzhl-src"
COMP="$ROOT/Code/Libraries/Source/Compression"

if [[ ! -f "$LZHL_SRC/src/lzhl.cpp" ]]; then
  echo "warn: liblzhl not found at $LZHL_SRC" >&2
else
  dst="$COMP/LZHCompress/CompLibSource"
  hdr="$COMP/LZHCompress/CompLibHeader"
  mkdir -p "$dst" "$hdr"
  for pair in "huff.cpp:Huff.cpp" "lz.cpp:Lz.cpp" "lzhl.cpp:Lzhl.cpp"; do
    src="${pair%%:*}"
    out="${pair##*:}"
    cp -f "$LZHL_SRC/src/$src" "$dst/$out"
    cp -f "$LZHL_SRC/src/_${src%%.cpp}.h" "$dst/" 2>/dev/null || true
  done
  cp -f "$LZHL_SRC/src/"*.tbl "$dst/" 2>/dev/null || true
  cp -f "$LZHL_SRC/include/lzhl/lzhl.h" "$hdr/lzhl.h"
  mkdir -p "$hdr/lzhl"
  cp -f "$LZHL_SRC/include/lzhl/lzhl.h" "$hdr/lzhl/lzhl.h"
  ln -sfn NoxCompress.h "$COMP/LZHCompress/Noxcompress.h"
  # Generals .dsp lists Lzhl_tcp.cpp; modern liblzhl does not ship it — empty TU is enough.
  if [[ ! -f "$dst/Lzhl_tcp.cpp" ]]; then
    echo '/* Lzhl_tcp.cpp — not used by liblzhl */' > "$dst/Lzhl_tcp.cpp"
  fi
  echo "populate: LZH CompLib ($(ls "$dst" | wc -l) files)"
fi

# Z_PREFIX zlib wrapper (links against system libz via meson dependency).
mkdir -p "$COMP/ZLib"
cat > "$COMP/ZLib/zlib.h" <<'EOF'
#ifndef GENERALS_ZLIB_WRAPPER_H
#define GENERALS_ZLIB_WRAPPER_H
#define Z_PREFIX 1
#include <zlib.h>
#endif
EOF
echo "populate: $COMP/ZLib/zlib.h"

# VC6 case aliases under Libraries/Include/Lib
lib_inc="$ROOT/Code/Libraries/Include/Lib"
mkdir -p "$lib_inc"
for alias in Basetype basetype Trig; do
  if [[ ! -e "$lib_inc/$alias.h" ]]; then
    base="$(echo "$alias" | sed 's/^./\U&/')" 
    if [[ "$alias" == "basetype" ]]; then base="BaseType"; fi
    if [[ -f "$lib_inc/${base}.h" ]]; then
      ln -sf "${base}.h" "$lib_inc/$alias.h"
    fi
  fi
done
