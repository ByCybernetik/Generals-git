wwutil_sources = files(
  'mathutil.cpp',
  'miscutil.cpp',
  'linux/miscutil_linux.cpp',
  'linux/stackdump_stub.cpp',
)

inc = include_directories('.', '../WWLib', '../WWDebug', '../WWMath')

wwutil_lib = static_library(
  'wwutil',
  wwutil_sources,
  include_directories : inc,
  cpp_args : generals_cpp_args,
  link_args : generals_link_args,
)
