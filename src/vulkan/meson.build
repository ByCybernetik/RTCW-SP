vulkan_dep = dependency('vulkan')

vk_renderer_sources = files(
    'vk_init.c',
    'vk_renderer.c',
    'vk_renderer_draw.c',
    'vk_material.c',
    'vk_surface.c',
    'vk_math.c',
    'vk_sky.c',
)

vk_renderer_inc = include_directories(
    '.',
    '..',
    '../qcommon',
    '../renderer',
    '../game',
)

vk_renderer = static_library('vk_renderer',
    vk_renderer_sources,
    c_args: ['-DVULKAN_BACKEND'],
    include_directories: vk_renderer_inc,
    dependencies: vulkan_dep,
)
