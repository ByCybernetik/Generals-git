# GameSpy SDK stub for Linux — headers under stub/include + no-op C API.

gamespy_lib = static_library(
  'gamespy',
  files('gamespy_stubs.c'),
  include_directories : include_directories(
    'stub/include',
    'stub/include/GameSpy',
    'stub/include/gamespy',
  ),
  c_args : [
    '-DNDEBUG',
  ],
  link_args : generals_link_args,
)
