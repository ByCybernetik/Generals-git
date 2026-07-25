# Linux deps: SDL3 + Vulkan + vendored D3D8 headers (from Renegade third_party/).

sdl3_dep = dependency('sdl3', required : true)
vulkan_dep = dependency('vulkan', required : true)

_dxsdk8_inc_path = meson.project_source_root() / 'third_party' / 'dxsdk8' / 'include'
_dxsdk8_compat_path = meson.project_source_root() / 'third_party' / 'dxsdk8' / 'include_compat'
_dxvk_native_windows_inc_path = meson.project_source_root() / 'third_party' / 'dxvk' / 'include' / 'native' / 'windows'

_d3d_header_inc_args = [
  '-I' + _dxsdk8_inc_path,
  '-I' + _dxsdk8_compat_path,
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

_ffmpeg_deps = []
if not get_option('disable_bink')
  _ffmpeg_deps += [
    dependency('libavcodec', required : true),
    dependency('libavformat', required : true),
    dependency('libavutil', required : true),
    dependency('libswscale', required : true),
    dependency('libswresample', required : true),
  ]
endif
if not get_option('disable_audio')
  _ffmpeg_deps += [
    dependency('libavcodec', required : true),
    dependency('libavformat', required : true),
    dependency('libavutil', required : true),
    dependency('libswresample', required : true),
  ]
endif

linux_platform_deps = [sdl3_dep, vulkan_dep, linux_graphics_dep, zlib_dep] + _ffmpeg_deps
