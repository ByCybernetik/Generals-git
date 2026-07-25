wwsaveload_sources = files(
  'definition.cpp',
  'definitionfactory.cpp',
  'definitionfactorymgr.cpp',
  'definitionmgr.cpp',
  'parameter.cpp',
  'persistfactory.cpp',
  'pointerremap.cpp',
  'saveload.cpp',
  'saveloadstatus.cpp',
  'saveloadsubsystem.cpp',
  'twiddler.cpp',
  'wwsaveload.cpp',
)

inc = include_directories('.', '../WWLib', '../WWLib/mingw', '../WWDebug', '../WWMath')

wwsaveload_lib = static_library(
  'wwsaveload',
  wwsaveload_sources,
  include_directories : inc,
  cpp_args : generals_cpp_args,
  link_args : generals_link_args,
)
