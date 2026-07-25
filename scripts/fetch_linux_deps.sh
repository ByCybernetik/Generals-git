#!/usr/bin/env bash
# Fetch/vendor Linux build dependencies for the Generals Meson port.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEPS="${GENERALS_DEPS_DIR:-/tmp/build-deps}"
TP="$ROOT/third_party"

mkdir -p "$DEPS" "$TP"

if [[ -f "$ROOT/.gitmodules" ]]; then
  echo "==> Initializing git submodules (imgui, Vulkan-Headers, VulkanMemoryAllocator)"
  git -C "$ROOT" submodule update --init --depth 1 \
    third_party/imgui \
    third_party/Vulkan-Headers \
    third_party/VulkanMemoryAllocator || true
fi

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
clone_or_update https://github.com/TheAssemblyArmada/liblzhl.git "$DEPS/liblzhl"
clone_or_update https://github.com/ByCybernetik/CnC_Renegade-main.git "$DEPS/CnC_Renegade-main"
clone_or_update https://github.com/ByCybernetik/CnC_Renegade-linux.git "$DEPS/CnC_Renegade-linux"
if [[ ! -d "$TP/VulkanMemoryAllocator/.git" ]]; then
  clone_or_update https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git "$DEPS/VulkanMemoryAllocator"
fi

echo "==> Linking third_party/"
mkdir -p "$TP/vma"
if [[ -d "$TP/VulkanMemoryAllocator/include" ]]; then
  ln -sfn "../VulkanMemoryAllocator/include" "$TP/vma/include"
else
  ln -sfn "$DEPS/VulkanMemoryAllocator/include" "$TP/vma/include"
fi
if [[ ! -d "$TP/imgui/.git" ]]; then
  clone_or_update https://github.com/ocornut/imgui.git "$DEPS/imgui"
  ln -sfn "$DEPS/imgui" "$TP/imgui"
fi
ln -sfn "$DEPS/CnC_Renegade-main/third_party/dxsdk8" "$TP/dxsdk8" 2>/dev/null || true

# D3D8/D3DX8 headers are vendored in third_party/d3d8_include (no external SDK required).

# Minimal DXVK native windows shims (already in-tree if present).
mkdir -p "$TP/dxvk/include/native/windows"
if [[ ! -f "$TP/dxvk/include/native/windows/windows.h" ]]; then
  echo "WARNING: third_party/dxvk shims missing; ensure they are committed or restored."
fi

# Case-insensitive D3DX header aliases for Linux (optional refresh if d3d8_include changes).
COMPAT="$TP/d3d8_include_compat"
INC="$TP/d3d8_include"
if [[ -d "$INC" ]]; then
  mkdir -p "$COMPAT"
  link_alias() {
    local base="$1"
    shift
    for alias in "$@"; do
      ln -sfn "../d3d8_include/$base" "$COMPAT/$alias"
    done
  }
  link_alias d3dx8math.h \
    d3dx8math.h D3dx8math.h D3dx8Math.h D3DX8Math.h D3DX8MATH.H D3dX8math.h Dx8math.h
  link_alias d3dx8core.h \
    d3dx8core.h D3dx8core.h D3dx8Core.h D3DX8Core.h D3DX8CORE.H D3dX8core.h Dx8core.h
  link_alias d3dx8tex.h \
    d3dx8tex.h D3dx8tex.h D3dx8Tex.h D3DX8Tex.h D3DX8TEX.H D3dX8tex.h Dx8tex.h
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
# Generals uses CompLibHeader/lzhl.h, not liblzhl's installed <lzhl/lzhl.h> layout.
if [[ -f "$LZH_SRC/Lzhl.cpp" ]]; then
  sed -i 's|#include <lzhl/lzhl.h>|#include "lzhl.h"|' "$LZH_SRC/Lzhl.cpp"
fi
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
