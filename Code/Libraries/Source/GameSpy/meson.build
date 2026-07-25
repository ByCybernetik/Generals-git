# GameSpy — stub SDK only (no vendor sources).

inc = include_directories(
  'GameSpy',
  'stub/include',
  'stub/include/gamespy',
  '.',
)

gamespy_lib = static_library(
  'gamespy',
  'linux/gamespy_stubs.cpp',
  include_directories : inc,
  cpp_args : ['-D_LIB', '-DGENERALS_GAMESPY_STUB'],
  link_args : generals_link_args,
)
