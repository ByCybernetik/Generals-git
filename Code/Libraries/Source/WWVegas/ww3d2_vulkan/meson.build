# WW3D2 Vulkan backend (vendored under Generals).

glslc = find_program('glslc', required : true)

_shader_src_dir = meson.current_source_dir() / 'shaders'
_shader_build_dir = meson.current_build_dir()
_shader_deploy_dir = meson.project_build_root() / 'shaders'

_glslc_inc = ['-I', _shader_src_dir]

mesh_vert_spv = custom_target(
  'ww3d_vulkan_mesh_vert_spv',
  input : 'shaders/mesh_textured.vert',
  output : 'mesh_textured.vert.spv',
  command : [glslc, '-fshader-stage=vertex'] + _glslc_inc + ['@INPUT@', '-o', '@OUTPUT@'],
  build_by_default : true,
)

mesh_frag_spv = custom_target(
  'ww3d_vulkan_mesh_frag_spv',
  input : 'shaders/mesh_textured.frag',
  output : 'mesh_textured.frag.spv',
  command : [glslc, '-fshader-stage=fragment'] + _glslc_inc + ['@INPUT@', '-o', '@OUTPUT@'],
  build_by_default : true,
)

simple2d_vert_spv = custom_target(
  'ww3d_vulkan_simple2d_vert_spv',
  input : 'shaders/simple_2d.vert',
  output : 'simple_2d.vert.spv',
  command : [glslc, '-fshader-stage=vertex'] + _glslc_inc + ['@INPUT@', '-o', '@OUTPUT@'],
  build_by_default : true,
)

simple2d_frag_spv = custom_target(
  'ww3d_vulkan_simple2d_frag_spv',
  input : 'shaders/simple_2d.frag',
  output : 'simple_2d.frag.spv',
  command : [glslc, '-fshader-stage=fragment'] + _glslc_inc + ['@INPUT@', '-o', '@OUTPUT@'],
  build_by_default : true,
)

terrain_vert_spv = custom_target(
  'ww3d_vulkan_terrain_vert_spv',
  input : 'shaders/terrain.vert',
  output : 'terrain.vert.spv',
  command : [glslc, '-fshader-stage=vertex'] + _glslc_inc + ['@INPUT@', '-o', '@OUTPUT@'],
  build_by_default : true,
)

terrain_frag_spv = custom_target(
  'ww3d_vulkan_terrain_frag_spv',
  input : 'shaders/terrain.frag',
  output : 'terrain.frag.spv',
  command : [glslc, '-fshader-stage=fragment'] + _glslc_inc + ['@INPUT@', '-o', '@OUTPUT@'],
  build_by_default : true,
)

shader_deploy_stamp = custom_target(
  'deploy_vulkan_shaders',
  input : [mesh_vert_spv, mesh_frag_spv, simple2d_vert_spv, simple2d_frag_spv, terrain_vert_spv, terrain_frag_spv],
  output : 'shader_deploy.stamp',
  command : [
    find_program('bash'), '-c',
    # Meson only substitutes @INPUT@/@OUTPUT@ as separate argv tokens.
    'mkdir -p "$1" && cp "$2" "$1/mesh_textured.vert.spv" && cp "$3" "$1/mesh_textured.frag.spv" && cp "$4" "$1/simple_2d.vert.spv" && cp "$5" "$1/simple_2d.frag.spv" && cp "$6" "$1/terrain.vert.spv" && cp "$7" "$1/terrain.frag.spv" && touch "$8"',
    '_',
    _shader_deploy_dir,
    '@INPUT0@',
    '@INPUT1@',
    '@INPUT2@',
    '@INPUT3@',
    '@INPUT4@',
    '@INPUT5@',
    '@OUTPUT@',
  ],
  build_by_default : true,
)

ww3d2_vulkan_sources = files(
  'vulkan_render_device.cpp',
  'vk_native_render_state.cpp',
  'vk_2d_renderer.cpp',
  'stb_texture.cpp',
  'vk_allocator.cpp',
  'vk_buffer.cpp',
  'vk_context.cpp',
  'renderdoc_capture.cpp',
  'vk_dx8_bridge.cpp',
  'vk_dx8_state.cpp',
  'vk_dx8_texture.cpp',
  'vk_shadow_pass.cpp',
  'vk_cpu_surface.cpp',
  'vk_format.cpp',
  'vk_framebuffer.cpp',
  'vk_gpu_buffer.cpp',
  'vk_pipeline.cpp',
  'vk_render_pass.cpp',
  'vk_renderer.cpp',
  'vk_shader.cpp',
  'vk_swapchain.cpp',
  'vk_staging_pool.cpp',
  'vk_texture.cpp',
  'vk_upload.cpp',
  'ww3d_vulkan.cpp',
)

ww3d2_vulkan_inc = include_directories(
  '.',
  '../../../../platform',
  '../../../../platform/linux',
  '../WW3D2',
  '../WWMath',
  '../WWLib',
  '../WWDebug',
  '../../../../../third_party/stb',
  '../../../../../third_party/VulkanMemoryAllocator/include',
  '../../../../../third_party/renderdoc',
)

_linux_d3dx_compat = meson.project_source_root() / 'Code' / 'platform' / 'linux' / 'linux_d3dx_compat.h'

ww3d2_vulkan_cpp_args = [
  '-std=gnu++11',
  '-DGENERALS_LINUX',
  '-DRENEGADE_LINUX',
  '-DRENEGADE_VULKAN',
  '-DGENERALS_VULKAN',
  '-DRENEGADE_WW3D2_BUILD',
  '-DNDEBUG',
  '-fpermissive',
  '-include', _linux_d3dx_compat,
  '-DRENEGADE_VULKAN_SHADER_DIR="@0@"'.format(_shader_build_dir),
]

ww3d2_vulkan_lib = static_library(
  'ww3d2_vulkan',
  ww3d2_vulkan_sources + [mesh_vert_spv, mesh_frag_spv, simple2d_vert_spv, simple2d_frag_spv, terrain_vert_spv, terrain_frag_spv, shader_deploy_stamp],
  include_directories : ww3d2_vulkan_inc,
  cpp_args : ww3d2_vulkan_cpp_args,
  dependencies : [sdl3_dep, vulkan_dep],
  link_args : generals_link_args,
  override_options : ['cpp_std=gnu++11'],
)

ww3d2_vulkan_dep = declare_dependency(
  link_with : ww3d2_vulkan_lib,
  include_directories : ww3d2_vulkan_inc,
  compile_args : ww3d2_vulkan_cpp_args,
  dependencies : [sdl3_dep, vulkan_dep],
)
