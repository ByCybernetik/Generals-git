# C&C Generals — Linux port (SDL3 + Vulkan), using CnC_Renegade-main as reference.
#
#   ./meson-setup-linux.sh build-linux
#   ninja -C build-linux

project('cnc-generals', ['c', 'cpp'],
  version : '1.08',
  meson_version : '>= 0.56',
  default_options : [
    'cpp_std=gnu++11',
    'buildtype=debugoptimized',
    'warning_level=1',
  ],
)

_platform = get_option('platform')
_is_linux = _platform == 'linux'
_renegade = get_option('renegade_port_path')

if not _is_linux
  error('Generals Meson port currently supports only -Dplatform=linux. MinGW can be added later.')
endif

if get_option('disable_audio')
  add_global_arguments('-DGENERALS_DISABLE_AUDIO', language : 'cpp')
endif

if get_option('disable_bink')
  add_global_arguments('-DGENERALS_DISABLE_BINK', language : 'cpp')
endif

# Share Renegade platform shims (RENEGADE_LINUX + GENERALS_LINUX both defined).
generals_cpp_args = [
  '-std=gnu++11',
  '-DGENERALS_LINUX',
  '-DRENEGADE_LINUX',
  '-D_UNIX',
  '-DNDEBUG',
  '-fpermissive',
  '-fshort-wchar',
]
add_global_arguments(generals_cpp_args, language : 'cpp')

_platform_inc = meson.project_source_root() / 'Code' / 'platform'
add_global_arguments('-I' + _platform_inc, language : 'cpp')
_linux_shim_inc = _platform_inc / 'linux'
add_global_arguments('-I' + _linux_shim_inc, language : 'cpp')

generals_link_args = ['-pthread', '-ldl', '-lm']
generals_wwlib_cpp_args = generals_cpp_args + ['-DREGEX_MALLOC', '-DSTDC_HEADERS']
generals_platform_cpp_args = generals_cpp_args

# Aliases for Renegade meson modules (third_party/ symlink at project root).
renegade_cpp_args = generals_cpp_args
renegade_link_args = generals_link_args
renegade_wwlib_cpp_args = generals_wwlib_cpp_args
renegade_platform_cpp_args = generals_platform_cpp_args

if get_option('graphics_backend') == 'vulkan'
  add_global_arguments('-DGENERALS_VULKAN', language : 'cpp')
  add_global_arguments('-DRENEGADE_VULKAN', language : 'cpp')
  generals_platform_cpp_args += '-DRENEGADE_VULKAN'
endif

# Renegade deps/linux expects third_party/ at project root and platform at Code/platform.
subdir('Code/deps/linux')

# Platform layer (vendored under Code/platform/).
renegade_platform_cpp_args = generals_platform_cpp_args
subdir('Code/platform')

# Westwood static libs (Generals tree).
subdir('Code/Libraries/Source/WWVegas/WWLib')
subdir('Code/Libraries/Source/WWVegas/WWDebug')
subdir('Code/Libraries/Source/WWVegas/WWMath')
subdir('Code/Libraries/Source/WWVegas/Wwutil')
subdir('Code/Libraries/Source/WWVegas/WWSaveLoad')

if get_option('graphics_backend') == 'vulkan'
  subdir('Code/Libraries/Source/WWVegas/ww3d2_vulkan')
endif
subdir('Code/Libraries/Source/WWVegas/WW3D2')

# Phase 3: Compression, GameSpy SDK, GameEngine stack.
subdir('Code/Libraries/Source/Compression')
subdir('Code/Libraries/Source/GameSpy')
subdir('Code/Libraries/Source/Benchmark')
subdir('Code/GameEngine')
subdir('Code/GameEngineDevice')
subdir('Code/Main')
subdir('Code/Tools/WorldBuilderLinux')

summary('platform', _platform, section : 'Configuration')
summary('renegade_port', _renegade, section : 'Configuration')
summary('wwlib', wwlib_lib.full_path(), section : 'Libraries')
summary('wwsaveload', wwsaveload_lib.full_path(), section : 'Libraries')
summary('ww3d2', ww3d2_lib.full_path(), section : 'Libraries')
if get_option('graphics_backend') == 'vulkan'
  summary('ww3d2_vulkan', ww3d2_vulkan_lib.full_path(), section : 'Libraries')
endif
summary('compression', compression_lib.full_path(), section : 'Libraries')
summary('gamespy', gamespy_lib.full_path(), section : 'Libraries')
summary('gameengine', gameengine_lib.full_path(), section : 'Libraries')
summary('gameenginedevice', gameenginedevice_lib.full_path(), section : 'Libraries')
summary('generals', generals_exe.full_path(), section : 'Targets')
