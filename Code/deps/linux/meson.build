# Linux deps: SDL3 + Vulkan + in-tree D3D8 headers (no external DirectX SDK).

sdl3_dep = dependency('sdl3', required : true)
vulkan_dep = dependency('vulkan', required : true)

_d3d8_inc_path = meson.project_source_root() / 'third_party' / 'd3d8_include'
_d3d8_compat_path = meson.project_source_root() / 'third_party' / 'd3d8_include_compat'
_dxvk_native_windows_inc_path = meson.project_source_root() / 'third_party' / 'dxvk' / 'include' / 'native' / 'windows'

_d3d_header_inc_args = [
  '-I' + _d3d8_inc_path,
  '-I' + _d3d8_compat_path,
  '-I' + _dxvk_native_windows_inc_path,
]

linux_d3d_headers_dep = declare_dependency(
  dependencies : [sdl3_dep, vulkan_dep],
  compile_args : _d3d_header_inc_args,
)

linux_graphics_dep = linux_d3d_headers_dep

_linux_prelude = meson.project_source_root() / 'Code' / 'platform' / 'linux' / 'dxvk_prelude.h'
add_global_arguments(_d3d_header_inc_args + ['-include', _linux_prelude], language : 'cpp')

zlib_dep = dependency('zlib', required : true)

subdir('ffmpeg')

linux_platform_deps = [sdl3_dep, vulkan_dep, linux_graphics_dep, zlib_dep]
if _ffmpeg_want
  linux_platform_deps += _ffmpeg_dep_list
endif
