# GameEngine — core RTS logic (VC6 GameEngine.dsp).

fs = import('fs')
_py3 = find_program('python3', required : true)

_dsp_list = run_command(
  _py3,
  meson.project_source_root() / 'scripts/dsp_sources_list.py',
  meson.current_source_dir() / 'GameEngine.dsp',
  check : true,
)
_linux_ge_skip = [
  'Source/GameNetwork/GameSpy.cpp',
  'Source/GameNetwork/GameSpyGameInfo.cpp',
]
gameengine_sources = []
foreach _p : _dsp_list.stdout().strip().split('\n')
  if _p != ''
    if host_machine.system() == 'linux' and _p in _linux_ge_skip
      continue
    endif
    gameengine_sources += files(_p)
  endif
endforeach
if host_machine.system() == 'linux'
  gameengine_sources += files('Source/GameNetwork/GameSpy_linux_stub.cpp')
  gameengine_sources += files('../platform/linux/linux_game_memory.cpp')
  gameengine_sources += files('../platform/linux/frame_profile.cpp')
endif

_ge_inc = [
  'Include',
  'Include/Precompiled',
  '../Libraries/Include',
  '../Libraries/Source/Compression',
  '../Libraries/Source/GameSpy',
  '../Libraries/Source/GameSpy/stub/include/gamespy',
  '../Libraries/Source/WWVegas',
  '../Libraries/Source/WWVegas/WWLib',
  '../Libraries/Source/WWVegas/WWDebug',
  '../Libraries/Source/WWVegas/WWMath',
  '../Libraries/Source/WWVegas/Wwutil',
  '../platform',
  '../platform/linux',
]

inc = include_directories(_ge_inc)

_d3dx_compat = meson.current_source_dir() / '../platform/linux/dxvk_compat.h'
_ge_cpp = generals_cpp_args + [
  '-D_LIB',
  '-D_MBCS',
  '-DZ_PREFIX',
  '-DRENEGADE_USE_D3DX8',
  '-include', _d3dx_compat,
]

gameengine_lib = static_library(
  'gameengine',
  gameengine_sources,
  include_directories : inc,
  cpp_args : _ge_cpp,
  link_args : generals_link_args,
  link_with : [
    compression_lib,
    gamespy_lib,
    wwlib_lib,
    wwdebug_lib,
    wwmath_lib,
    wwutil_lib,
  ],
)
