# generals — Linux entry via Renegade platform (WinMain wrapper).

inc = include_directories(
  '.',
  '../GameEngine/Include',
  '../GameEngine/Include/Precompiled',
  '../GameEngineDevice/Include',
  '../GameEngineDevice/linux',
  '../Libraries/Include',
  '../Libraries/Source/WWVegas',
  '../Libraries/Source/WWVegas/WW3D2',
  '../Libraries/Source/WWVegas/WWLib',
  '../Libraries/Source/WWVegas/WWDebug',
  '../Libraries/Source/WWVegas/WWMath',
  '../Libraries/Source/GameSpy',
  '../Libraries/Source/GameSpy/stub/include/gamespy',
  '../Libraries/Source/WWVegas/WWDownload',
  '../Libraries/Source/WWVegas/WWSaveLoad',
  '../platform',
  '../platform/linux',
  '../Libraries/Source/WWVegas/ww3d2_vulkan',
)

_prerts = meson.project_source_root() / 'Code/GameEngine/Include/Precompiled/PreRTS.h'
_d3dx_compat = meson.project_source_root() / 'Code/platform/linux/dxvk_compat.h'
_main_cpp = generals_cpp_args + [
  '-DRENEGADE_USE_D3DX8',
  '-include', _prerts,
  '-include', _d3dx_compat,
]

generals_exe = executable(
  'generals',
  files('WinMain.cpp', '../platform/linux_entry.cpp', '../platform/linux/generals_link_stubs.cpp'),
  include_directories : inc,
  cpp_args : _main_cpp,
  link_args : generals_link_args,
  link_with : [
    gameenginedevice_lib,
    gameengine_lib,
    ww3d2_lib,
    wwsaveload_lib,
    wwlib_lib,
    wwdebug_lib,
    wwmath_lib,
    wwutil_lib,
    compression_lib,
    gamespy_lib,
    benchmark_lib,
    platform_lib,
  ] + (get_option('graphics_backend') == 'vulkan' ? [ww3d2_vulkan_lib] : []),
  dependencies : linux_platform_deps,
)
