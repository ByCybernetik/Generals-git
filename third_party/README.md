# third_party

## Git submodules

Initialize after clone:

```bash
git submodule update --init --recursive
```

| Path | Repository |
|------|------------|
| `Vulkan-Headers` | https://github.com/KhronosGroup/Vulkan-Headers |
| `VulkanMemoryAllocator` | https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator |
| `imgui` | (see `.gitmodules`) |

Vulkan headers: `third_party/Vulkan-Headers/include`

VMA header: `third_party/VulkanMemoryAllocator/include/vk_mem_alloc.h`

STB headers (vendored from https://github.com/planetack/stb_image): `third_party/stb/`
(`stb_image.h`, `stb_dxt.h`, `stb_truetype.h` only)

D3D8/D3DX8 headers (compile-time only, Linux port): `third_party/d3d8_include/`

Linux port tooling (Meson, GameSpy stubs, `fetch_linux_deps.sh`) is maintained on branch
`cursor/linux-build-setup-9732`, not on `main`.
