# D3D8 / D3DX8 headers (compile-time only)

Minimal Direct3D 8 SDK headers used by the Linux Vulkan port. Combined with the
Win32 shims in `Code/platform/linux/`, they replace an external DirectX SDK
install.

Case-insensitive filename aliases (`D3dx8math.h`, `D3DX8MATH.H`, etc.) live in
this same directory as symlinks to `d3dx8math.h`, `d3dx8core.h`, and
`d3dx8tex.h`.

No download step is required — these files are committed in-tree.
