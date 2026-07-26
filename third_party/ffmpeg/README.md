# Minimal FFmpeg (Linux port)

Vendored **static** FFmpeg build for Bink video and SDL audio decode (webm, bink, wav, mp3, ogg).

Sources and binaries are not committed. After clone:

```bash
./scripts/build_minimal_ffmpeg.sh
```

Install tree: `third_party/ffmpeg/install/` (headers + `.a` + pkg-config).

Meson uses it automatically when present (`-Dbundled_ffmpeg=auto`, default). Force system FFmpeg with `-Dbundled_ffmpeg=system`, or require the bundle with `-Dbundled_ffmpeg=bundled`.

Supported formats (decode only):

| Container | Demuxer | Decoders |
|-----------|---------|----------|
| Bink `.bik` | `bink` | `bink`, `binkaudio_dct`, `binkaudio_rdft` |
| WebM | `matroska` / `webm` | `vp8`, `vp9`, `opus`, `vorbis` |
| WAV | `wav` | PCM variants, MS ADPCM, IMA ADPCM |
| MP3 | `mp3` | `mp3float` |
| Ogg | `ogg` | `vorbis` |

Default FFmpeg version: **6.1.1** (`FFMPEG_VERSION=7.0.2` to override).
