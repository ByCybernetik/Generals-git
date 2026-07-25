# Building Generals on Linux

## Dependencies

System packages: `meson`, `ninja-build`, `g++`, `pkg-config`, `libvulkan-dev`,
`glslc`, FFmpeg (`libavcodec-dev` … `libswresample-dev`), `zlib1g-dev`.

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
