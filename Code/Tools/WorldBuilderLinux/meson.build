# WorldBuilder Linux (ImGui) — Milestone 1

imgui_sources = files(
  '../../../third_party/imgui/imgui.cpp',
  '../../../third_party/imgui/imgui_demo.cpp',
  '../../../third_party/imgui/imgui_draw.cpp',
  '../../../third_party/imgui/imgui_tables.cpp',
  '../../../third_party/imgui/imgui_widgets.cpp',
  '../../../third_party/imgui/backends/imgui_impl_sdl3.cpp',
  '../../../third_party/imgui/backends/imgui_impl_vulkan.cpp',
)

wb_shader_vert = custom_target(
  'wb_map_mesh_vert',
  input : 'shaders/map_mesh.vert',
  output : 'map_mesh.vert.spv',
  command : ['glslc', '@INPUT@', '-o', '@OUTPUT@'],
)
wb_shader_frag = custom_target(
  'wb_map_mesh_frag',
  input : 'shaders/map_mesh.frag',
  output : 'map_mesh.frag.spv',
  command : ['glslc', '@INPUT@', '-o', '@OUTPUT@'],
)
wb_road_vert = custom_target(
  'wb_map_road_vert',
  input : 'shaders/map_road.vert',
  output : 'map_road.vert.spv',
  command : ['glslc', '@INPUT@', '-o', '@OUTPUT@'],
)
wb_road_frag = custom_target(
  'wb_map_road_frag',
  input : 'shaders/map_road.frag',
  output : 'map_road.frag.spv',
  command : ['glslc', '@INPUT@', '-o', '@OUTPUT@'],
)
wb_object_vert = custom_target(
  'wb_map_object_vert',
  input : 'shaders/map_object.vert',
  output : 'map_object.vert.spv',
  command : ['glslc', '@INPUT@', '-o', '@OUTPUT@'],
)
wb_object_frag = custom_target(
  'wb_map_object_frag',
  input : 'shaders/map_object.frag',
  output : 'map_object.frag.spv',
  command : ['glslc', '@INPUT@', '-o', '@OUTPUT@'],
)
wb_water_vert = custom_target(
  'wb_map_water_vert',
  input : 'shaders/map_water.vert',
  output : 'map_water.vert.spv',
  command : ['glslc', '@INPUT@', '-o', '@OUTPUT@'],
)
wb_water_frag = custom_target(
  'wb_map_water_frag',
  input : 'shaders/map_water.frag',
  output : 'map_water.frag.spv',
  command : ['glslc', '@INPUT@', '-o', '@OUTPUT@'],
)

wb_linux_sources = files(
  'src/main.cpp',
  'src/VulkanHost.cpp',
  'src/EditorApp.cpp',
  'src/MapDocument.cpp',
  'src/MapOpen.cpp',
  'src/HeightmapPreview.cpp',
  'src/MapViewport.cpp',
  'src/RoadGeometry.cpp',
  'src/ObjectMeshCache.cpp',
  'src/WaterGeometry.cpp',
  'src/ui/MainUi.cpp',
) + imgui_sources

wb_linux_inc = include_directories(
  'include',
  '../../../third_party/imgui',
  '../../../third_party/imgui/backends',
  '../../GameEngine/Include',
  '../../GameEngine/Include/Precompiled',
  '../../GameEngineDevice/Include',
  '../../GameEngineDevice/linux',
  '../../Libraries/Include',
  '../../Libraries/Source/WWVegas',
  '../../Libraries/Source/WWVegas/WW3D2',
  '../../Libraries/Source/WWVegas/WWLib',
  '../../Libraries/Source/WWVegas/WWDebug',
  '../../Libraries/Source/WWVegas/WWMath',
  '../../Libraries/Source/GameSpy',
  '../../Libraries/Source/GameSpy/stub/include/gamespy',
  '../../Libraries/Source/WWVegas/WWDownload',
  '../../Libraries/Source/WWVegas/WWSaveLoad',
  '../../platform',
  '../../platform/linux',
  '../../Libraries/Source/WWVegas/ww3d2_vulkan',
)

_prerts = meson.project_source_root() / 'Code/GameEngine/Include/Precompiled/PreRTS.h'
_d3dx_compat = meson.project_source_root() / 'Code/platform/linux/dxvk_compat.h'

wb_linux_cpp = generals_cpp_args + [
  '-DRENEGADE_USE_D3DX8',
  '-DIMGUI_DEFINE_MATH_OPERATORS',
  '-DWB_SHADER_DIR="@0@"'.format(meson.current_build_dir()),
  '-include', _prerts,
  '-include', _d3dx_compat,
]

worldbuilder_exe = executable(
  'worldbuilder',
  wb_linux_sources + files('../../platform/linux/generals_link_stubs.cpp'),
  include_directories : wb_linux_inc,
  cpp_args : wb_linux_cpp,
  link_args : generals_link_args,
  link_depends : [wb_shader_vert, wb_shader_frag, wb_road_vert, wb_road_frag, wb_object_vert, wb_object_frag,
    wb_water_vert, wb_water_frag],
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
  install : false,
)

summary('worldbuilder', worldbuilder_exe.full_path(), section : 'Targets')
