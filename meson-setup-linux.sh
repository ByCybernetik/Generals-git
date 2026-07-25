#!/usr/bin/env bash
# Configure Meson for Linux. Builds minimal FFmpeg when using bundled mode.
#
# Usage:
#   ./meson-setup-linux.sh build
#   ./meson-setup-linux.sh build -Dbundled_ffmpeg=system
#   FFMPEG_MODE=system ./meson-setup-linux.sh build

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-build}"
shift || true

FFMPEG_MODE="${FFMPEG_MODE:-auto}"
for arg in "$@"; do
  case "$arg" in
    -Dbundled_ffmpeg=bundled) FFMPEG_MODE=bundled ;;
    -Dbundled_ffmpeg=system) FFMPEG_MODE=system ;;
    -Dbundled_ffmpeg=auto) FFMPEG_MODE=auto ;;
  esac
done

FFMPEG_PC="$ROOT/third_party/ffmpeg/install/lib/pkgconfig/libavcodec.pc"

need_ffmpeg_build() {
  case "$FFMPEG_MODE" in
    bundled) return 0 ;;
    auto) [[ ! -f "$FFMPEG_PC" ]] && return 0 ;;
  esac
  return 1
}

if need_ffmpeg_build; then
  echo "Minimal FFmpeg not found; running scripts/build_minimal_ffmpeg.sh ..."
  "$ROOT/scripts/build_minimal_ffmpeg.sh"
fi

exec meson setup "$BUILD_DIR" -Dplatform=linux -Dgraphics_backend=vulkan -Dbundled_ffmpeg="$FFMPEG_MODE" "$@"
