#!/usr/bin/env bash
# Fetch/vendor Linux build dependencies for the Generals Meson port.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${GENERALS_DEPS_DIR:-/tmp/build-deps}"
TP="$ROOT/third_party"

mkdir -p "$DEPS" "$TP"

clone_or_update() {
  local url="$1" dest="$2" ref="${3:-}"
  if [[ -d "$dest/.git" ]]; then
    git -C "$dest" fetch --depth 1 origin ${ref:+"$ref"} || true
  else
    rm -rf "$dest"
    if [[ -n "$ref" ]]; then
      git clone --depth 1 --branch "$ref" "$url" "$dest" || git clone --depth 1 "$url" "$dest"
    else
      git clone --depth 1 "$url" "$dest"
    fi
  fi
}

echo "==> Cloning dependency sources into $DEPS"
clone_or_update https://github.com/nothings/stb.git "$DEPS/stb"
clone_or_update https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git "$DEPS/VulkanMemoryAllocator"
clone_or_update https://github.com/ocornut/imgui.git "$DEPS/imgui"
clone_or_update https://github.com/TheAssemblyArmada/liblzhl.git "$DEPS/liblzhl"
clone_or_update https://github.com/ByCybernetik/CnC_Renegade-main.git "$DEPS/CnC_Renegade-main"
clone_or_update https://github.com/ByCybernetik/CnC_Renegade-linux.git "$DEPS/CnC_Renegade-linux"

echo "==> Linking third_party/"
ln -sfn "$DEPS/stb" "$TP/stb"
ln -sfn "$DEPS/imgui" "$TP/imgui"
mkdir -p "$TP/vma"
ln -sfn "$DEPS/VulkanMemoryAllocator/include" "$TP/vma/include"
ln -sfn "$DEPS/CnC_Renegade-main/third_party/dxsdk8" "$TP/dxsdk8"

# Minimal DXVK native windows shims (already in-tree if present).
mkdir -p "$TP/dxvk/include/native/windows"
if [[ ! -f "$TP/dxvk/include/native/windows/windows.h" ]]; then
  echo "WARNING: third_party/dxvk shims missing; ensure they are committed or restored."
fi

# Case-insensitive D3DX header aliases for Linux.
COMPAT="$TP/dxsdk8/include_compat"
if [[ -d "$TP/dxsdk8/include" ]]; then
  mkdir -p "$COMPAT"
  for base in D3DX8Math.h D3DX8Core.h D3DX8Tex.h; do
    src="$(find "$TP/dxsdk8/include" -iname "$base" | head -1 || true)"
    if [[ -n "$src" ]]; then
      for alias in "$base" "${base,,}" "${base^}" "$(echo "$base" | sed 's/DX/Dx/;s/Math/math/;s/Core/core/;s/Tex/tex/')"; do
        ln -sfn "$src" "$COMPAT/$alias" 2>/dev/null || cp -n "$src" "$COMPAT/$alias" 2>/dev/null || true
      done
    fi
  done
fi

# RenderDoc app header (single file).
mkdir -p "$TP/renderdoc"
if [[ ! -f "$TP/renderdoc/renderdoc_app.h" ]]; then
  curl -fsSL -o "$TP/renderdoc/renderdoc_app.h" \
    https://raw.githubusercontent.com/baldurk/renderdoc/v1.34/renderdoc/api/app/renderdoc_app.h
fi

echo "==> Populating LZHCompress CompLib from liblzhl"
LZH_HDR="$ROOT/Code/Libraries/Source/Compression/LZHCompress/CompLibHeader"
LZH_SRC="$ROOT/Code/Libraries/Source/Compression/LZHCompress/CompLibSource"
mkdir -p "$LZH_HDR" "$LZH_SRC"
cp -f "$DEPS/liblzhl/include/"*.h "$LZH_HDR/" 2>/dev/null || \
  cp -f "$DEPS/liblzhl/src/"_*.h "$LZH_HDR/" 2>/dev/null || true
# liblzhl layout varies; copy what Generals expects.
if [[ -f "$DEPS/liblzhl/include/Lzhl.h" ]]; then
  cp -f "$DEPS/liblzhl/include/Lzhl.h" "$LZH_HDR/Lzhl.h"
fi
for f in Lzhl.cpp Lz.cpp Huff.cpp hdec_g.tbl hdec_s.tbl hdisp.tbl henc.tbl \
         lzhl.cpp lz.cpp huff.cpp; do
  if [[ -f "$DEPS/liblzhl/src/$f" ]]; then
    # Normalize names Generals meson expects (PascalCase).
    case "$f" in
      lzhl.cpp) dest=Lzhl.cpp ;;
      lz.cpp) dest=Lz.cpp ;;
      huff.cpp) dest=Huff.cpp ;;
      *) dest="$f" ;;
    esac
    cp -f "$DEPS/liblzhl/src/$f" "$LZH_SRC/$dest"
  fi
done
# Headers used by sources
for f in _lzhl.h _lz.h _huff.h; do
  [[ -f "$DEPS/liblzhl/src/$f" ]] && cp -f "$DEPS/liblzhl/src/$f" "$LZH_SRC/$f"
done
# Ensure Lzhl.h exists for includes
if [[ ! -f "$LZH_HDR/Lzhl.h" && -f "$DEPS/liblzhl/src/_lzhl.h" ]]; then
  # Some trees expose public API differently; keep a thin wrapper if needed.
  echo '#pragma once
#include "../CompLibSource/_lzhl.h"' > "$LZH_HDR/Lzhl.h"
fi
: > "$LZH_SRC/Lzhl_tcp.cpp"

echo "==> Done."
echo "Build with:"
echo "  export PKG_CONFIG_PATH=/opt/sdl3/lib/pkgconfig\${PKG_CONFIG_PATH:+:\$PKG_CONFIG_PATH}"
echo "  CC=gcc CXX=g++ meson setup build-linux -Dplatform=linux -Dgraphics_backend=vulkan"
echo "  ninja -C build-linux Code/Main/generals"
