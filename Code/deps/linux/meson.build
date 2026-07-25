# Linux deps: SDL3 + Vulkan + vendored D3D8 headers (third_party/d3d8_include).

fs = import('fs')

sdl3_dep = dependency('sdl3', required : true)
vulkan_dep = dependency('vulkan', required : true)

_d3d8_inc_path = meson.project_source_root() / 'third_party' / 'd3d8_include'

_d3d_header_inc_args = [
  '-I' + _d3d8_inc_path,
]

linux_d3d_headers_dep = declare_dependency(
  dependencies : [sdl3_dep, vulkan_dep],
  compile_args : _d3d_header_inc_args,
)

linux_graphics_dep = linux_d3d_headers_dep

_linux_prelude = meson.project_source_root() / 'Code' / 'platform' / 'linux' / 'linux_prelude.h'
add_global_arguments(_d3d_header_inc_args + ['-include', _linux_prelude], language : 'cpp')

zlib_dep = dependency('zlib', required : true)

_ffmpeg_lib_names = []
if not get_option('disable_bink')
  _ffmpeg_lib_names += ['libavcodec', 'libavformat', 'libavutil', 'libswscale', 'libswresample']
endif
if not get_option('disable_audio')
  foreach _lib : ['libavcodec', 'libavformat', 'libavutil', 'libswresample']
    if _lib not in _ffmpeg_lib_names
      _ffmpeg_lib_names += _lib
    endif
  endforeach
endif

_ffmpeg_deps = []
_ffmpeg_bink_deps = []
if _ffmpeg_lib_names.length() > 0
  _bundled_opt = get_option('bundled_ffmpeg')
  _ffmpeg_install = meson.project_source_root() / 'third_party' / 'ffmpeg' / 'install'
  _ffmpeg_pc = _ffmpeg_install / 'lib' / 'pkgconfig'
  _use_bundled = false
  if _bundled_opt == 'bundled'
    if not fs.exists(_ffmpeg_pc / 'libavcodec.pc')
      error('bundled_ffmpeg=bundled but @0@/libavcodec.pc not found. Run scripts/build_minimal_ffmpeg.sh'.format(_ffmpeg_pc))
    endif
    _use_bundled = true
  elif _bundled_opt == 'auto' and fs.exists(_ffmpeg_pc / 'libavcodec.pc')
    _use_bundled = true
  endif

  foreach _lib : _ffmpeg_lib_names
    if _use_bundled
      _dep = dependency(_lib, required : true, static : true, pkg_config_path : [_ffmpeg_pc])
    else
      _dep = dependency(_lib, required : true)
    endif
    _ffmpeg_deps += _dep
    if _lib in ['libavcodec', 'libavformat', 'libavutil', 'libswscale', 'libswresample']
      _ffmpeg_bink_deps += _dep
    endif
  endforeach
endif

linux_platform_deps = [sdl3_dep, vulkan_dep, linux_graphics_dep, zlib_dep] + _ffmpeg_deps
