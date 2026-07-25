# GameEngineDevice — W3D / Win32 / media device layer (VC6 GameEngineDevice.dsp).

_py3 = find_program('python3', required : true)

_dsp_list = run_command(
  _py3,
  meson.project_source_root() / 'scripts/dsp_sources_list.py',
  meson.current_source_dir() / 'GameEngineDevice.dsp',
  check : true,
)
gameenginedevice_sources = []
foreach _p : _dsp_list.stdout().strip().split('\n')
  if _p != ''
    gameenginedevice_sources += files(_p)
  endif
endforeach

_disable_miles = get_option('disable_audio') or host_machine.system() == 'linux'
_disable_bink = get_option('disable_bink')
_use_sdl_audio = not get_option('disable_audio') and host_machine.system() == 'linux'
_ged_filtered = []
foreach _f : gameenginedevice_sources
  _path = '@0@'.format(_f)
  _skip_it = false
  if _disable_miles and _path.contains('MilesAudioManager.cpp')
    _skip_it = true
  endif
  if _disable_bink and _path.contains('BinkVideoPlayer.cpp')
    _skip_it = true
  endif
  if _path.contains('W3DWebBrowser.cpp')
    _skip_it = true
  endif
  if _path.contains('Win32DIKeyboard.cpp')
    _skip_it = true
  endif
  if _path.contains('Win32DIMouse.cpp')
    _skip_it = true
  endif
  if not _skip_it
    _ged_filtered += _f
  endif
endforeach

_ged_sources = _ged_filtered
if get_option('disable_audio')
  _ged_sources += files('linux/miles_audio_manager_stub.cpp')
elif host_machine.system() == 'linux'
  _ged_sources += files('linux/sdlaudio_manager.cpp')
endif
if _disable_bink
  _ged_sources += files('linux/bink_video_player_stub.cpp')
endif
_ged_sources += files('linux/w3d_web_browser_stub.cpp')
_ged_sources += files(
  'linux/win32_di_keyboard_stub.cpp',
  'linux/win32_di_mouse_stub.cpp',
)

_ged_inc = [
  'Include',
  '../GameEngine/Include',
  '../GameEngine/Include/Precompiled',
  '../Libraries/Include',
  '../Libraries/Include/Granny',
  '../Libraries/Include/MSS',
  '../Libraries/Source/Benchmark',
  '../Main',
  '../Libraries/Source/WWVegas',
  '../Libraries/Source/WWVegas/WW3D2',
  '../Libraries/Source/WWVegas/WWLib',
  '../Libraries/Source/WWVegas/WWDebug',
  '../Libraries/Source/WWVegas/WWMath',
  '../Libraries/Source/WWVegas/WWSaveLoad',
  '../platform',
  '../platform/linux',
  '../Libraries/Source/WWVegas/ww3d2_vulkan',
  'linux',
]

inc = include_directories(_ged_inc)

_d3dx_compat = meson.current_source_dir() / '../platform/linux/linux_d3dx_compat.h'
_prerts = meson.current_source_dir() / '../GameEngine/Include/Precompiled/PreRTS.h'
_ged_cpp = generals_cpp_args + [
  '-D_LIB',
  '-D_MBCS',
  '-DRENEGADE_USE_D3DX8',
  '-include', _prerts,
  '-include', _d3dx_compat,
]

_link_libs = [
  benchmark_lib,
  gameengine_lib,
  ww3d2_lib,
  wwsaveload_lib,
  wwlib_lib,
  wwdebug_lib,
  wwmath_lib,
  wwutil_lib,
]
if get_option('graphics_backend') == 'vulkan'
  _link_libs += ww3d2_vulkan_lib
endif

gameenginedevice_lib = static_library(
  'gameenginedevice',
  _ged_sources,
  include_directories : inc,
  cpp_args : _ged_cpp,
  link_args : generals_link_args,
  link_with : _link_libs,
)
