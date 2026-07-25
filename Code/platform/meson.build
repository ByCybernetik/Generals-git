platform_sources = files(
  'sdl3_host.cpp',
  'linux_entry.cpp',
  'linux/pe_resource_loader.cpp',
  'linux/win32_stubs.cpp',
  'linux/win32_extra_stubs.cpp',
  'linux/linux_registry_ini.cpp',
  'linux/linux_ani_cursor.cpp',
  'linux/linux_sync.cpp',
  'linux/winsock_stubs.cpp',
  'linux/vfw_stubs.cpp',
  'linux/ime_stubs.cpp',
)

if not get_option('disable_bink')
  platform_sources += files('linux/bink_ffmpeg.cpp')
endif

platform_inc = include_directories(
  '.',
  'linux',
  '../Libraries/Source/WWVegas/WWLib',
  '../Libraries/Source/WWVegas/WWDebug',
)

_platform_cpp = []
foreach _arg : renegade_platform_cpp_args
  if _arg != '-fshort-wchar'
    _platform_cpp += _arg
  endif
endforeach

platform_lib = static_library(
  'platform',
  platform_sources,
  include_directories : platform_inc,
  cpp_args : _platform_cpp,
  dependencies : [sdl3_dep] + (get_option('disable_bink') ? [] : _ffmpeg_dep_list),
  install : false,
)

platform_dep = declare_dependency(
  link_with : platform_lib,
  include_directories : platform_inc,
  compile_args : renegade_platform_cpp_args,
  dependencies : [sdl3_dep, linux_graphics_dep],
)
