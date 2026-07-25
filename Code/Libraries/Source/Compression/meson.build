# Compression — EAC codecs + LZH + system zlib (Z_PREFIX via ZLib/zlib.h shim).

zlib_dep = dependency('zlib', required : true)

_inc = include_directories(
  '.',
  'LZHCompress',
  'LZHCompress/CompLibHeader',
  'LZHCompress/CompLibSource',
  'EAC',
  '../../Include',
  '../../Include/Lib',
)

_cpp_args = ['-D_LIB', '-DZ_PREFIX']

_py3 = find_program('python3', required : true)

_dsp_list = run_command(
  _py3,
  meson.project_source_root() / 'scripts/dsp_sources_list.py',
  meson.current_source_dir() / 'Compression.dsp',
  check : true,
)
compression_sources = []
foreach _p : _dsp_list.stdout().strip().split('\n')
  if _p != ''
    # Linux uses system zlib; skip vendored ZLib/*.c from the .dsp.
    if host_machine.system() == 'linux' and _p.startswith('ZLib/')
      continue
    endif
    compression_sources += files(_p)
  endif
endforeach

if host_machine.system() == 'linux'
  compression_sources += files('linux/zlib_prefix_shim.cpp')
endif

compression_lib = static_library(
  'compression',
  compression_sources,
  include_directories : _inc,
  cpp_args : generals_cpp_args + _cpp_args,
  c_args : ['-D_LIB', '-DZ_PREFIX'],
  dependencies : [zlib_dep],
  link_args : generals_link_args,
)
