_wwdebug_inc = ['.', '../WWLib', '../WWLib/mingw', '../../../../platform', '../../../../platform/linux']
inc = include_directories(_wwdebug_inc)

wwdebug_lib = static_library(
  'wwdebug',
  files(
    'wwmemlog.cpp',
    'wwprofile.cpp',
    '../WWLib/mingw/wwdebug_stubs.cpp',
  ),
  include_directories : inc,
  cpp_args : generals_cpp_args,
  link_args : generals_link_args,
)
