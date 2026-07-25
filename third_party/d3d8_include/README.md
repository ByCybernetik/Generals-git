# D3D8 / D3DX8 headers (compile-time only)

Minimal Direct3D 8 SDK headers used by the Linux Vulkan port. Combined with
`third_party/dxvk/include/native/windows` shims, they replace an external
DirectX SDK install.

No `fetch_linux_deps.sh` step is required for these files — they are committed
in-tree.
