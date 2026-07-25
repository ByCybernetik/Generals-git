#!/usr/bin/env bash
# Build a minimal static FFmpeg for Generals Linux port:
#   webm (matroska), bink, wav, mp3, ogg (+ vorbis/opus/vp8/vp9 decoders).
#
# Output: third_party/ffmpeg/install/{include,lib,lib/pkgconfig}
#
# Usage:
#   ./scripts/build_minimal_ffmpeg.sh
#   FFMPEG_VERSION=7.0.2 ./scripts/build_minimal_ffmpeg.sh
#   JOBS=8 ./scripts/build_minimal_ffmpeg.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FFMPEG_VERSION="${FFMPEG_VERSION:-6.1.1}"
SRC_DIR="${FFMPEG_SRC_DIR:-$ROOT/third_party/ffmpeg/src/ffmpeg-$FFMPEG_VERSION}"
INSTALL_PREFIX="${FFMPEG_PREFIX:-$ROOT/third_party/ffmpeg/install}"
TARBALL="$ROOT/third_party/ffmpeg/src/ffmpeg-$FFMPEG_VERSION.tar.xz"
URL="https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

mkdir -p "$ROOT/third_party/ffmpeg/src"

if [[ ! -f "$SRC_DIR/configure" ]]; then
  echo "Downloading FFmpeg $FFMPEG_VERSION..."
  curl -fsSL -o "$TARBALL" "$URL"
  rm -rf "$SRC_DIR"
  tar -xf "$TARBALL" -C "$ROOT/third_party/ffmpeg/src"
fi

cd "$SRC_DIR"

echo "Configuring minimal FFmpeg -> $INSTALL_PREFIX"
./configure \
  --prefix="$INSTALL_PREFIX" \
  --disable-everything \
  --disable-programs \
  --disable-doc \
  --disable-network \
  --disable-autodetect \
  --disable-iconv \
  --disable-zlib \
  --disable-bzlib \
  --disable-lzma \
  --disable-hwaccels \
  --disable-x86asm \
  --enable-static \
  --disable-shared \
  --enable-small \
  --enable-pic \
  --enable-avcodec \
  --enable-avformat \
  --enable-avutil \
  --enable-swscale \
  --enable-swresample \
  --enable-protocol=file \
  --enable-demuxer=bink,matroska,webm,ogg,wav,mp3 \
  --enable-decoder=bink,binkaudio_dct,binkaudio_rdft,vp8,vp9,opus,vorbis,mp3float,pcm_s16le,pcm_s24le,pcm_s32le,pcm_s16be,pcm_s24be,pcm_s32be,pcm_f32le,pcm_f32be,pcm_u8,pcm_alaw,pcm_mulaw \
  --enable-parser=vorbis,opus,mpegaudio,vp9

echo "Building ($JOBS jobs)..."
make -j"$JOBS"
make install

echo ""
echo "Done. Installed to: $INSTALL_PREFIX"
echo "Meson will auto-detect it (bundled_ffmpeg=auto) or use -Dbundled_ffmpeg=bundled"
du -sh "$INSTALL_PREFIX" || true
ls -lh "$INSTALL_PREFIX/lib/"libav*.a "$INSTALL_PREFIX/lib/"libsw*.a 2>/dev/null || true
