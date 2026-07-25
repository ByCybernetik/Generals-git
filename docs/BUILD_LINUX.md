# Building Generals on Linux

## Dependencies

System packages: `meson`, `ninja-build`, `g++`, `pkg-config`, `libvulkan-dev`,
`glslc`, FFmpeg dev packages (`libavcodec-dev`, `libavformat-dev`, `libavutil-dev`,
`libswresample-dev`, and `libswscale-dev` when Bink is enabled), `zlib1g-dev`.

Any distro FFmpeg **4.4+** is supported (pkg-config names above; no pinned SONAME).

SDL3 is expected via pkg-config (e.g. install to `/opt/sdl3` and set
`PKG_CONFIG_PATH=/opt/sdl3/lib/pkgconfig`).

Fetch vendored third-party trees and LZH sources:

```bash
./scripts/fetch_linux_deps.sh
```

Regenerate GameSpy stub headers if needed:

```bash
python3 scripts/gen_gamespy_stub_headers.py
```

## Configure and build

```bash
export PKG_CONFIG_PATH=/opt/sdl3/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}
export LD_LIBRARY_PATH=/opt/sdl3/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
CC=gcc CXX=g++ meson setup build-linux -Dplatform=linux -Dgraphics_backend=vulkan
ninja -C build-linux Code/Main/generals
```

Binary: `build-linux/Code/Main/generals`.

GameSpy online APIs are stubbed (no live GameSpy servers).

## Prebuilt binary (CI)

After the [Linux build workflow](https://github.com/ByCybernetik/Generals-git/actions/workflows/linux-build.yml) runs on branch `cursor/linux-build-setup-9732`:

1. **GitHub Release (recommended):** [linux-preview](https://github.com/ByCybernetik/Generals-git/releases/tag/linux-preview) — download `generals-linux-x86_64-stripped.tar.gz`
2. **Workflow artifact:** open the latest successful run → **Artifacts** → `generals-linux-x86_64-stripped`

```bash
tar -xzf generals-linux-x86_64-stripped.tar.gz
chmod +x generals-stripped
export LD_LIBRARY_PATH=/path/to/sdl3/lib:$LD_LIBRARY_PATH
./generals-stripped
```

You still need SDL3 (`libSDL3.so.0`), Vulkan, and FFmpeg runtime libraries on the target machine (any FFmpeg 4.4+ from your distro).
