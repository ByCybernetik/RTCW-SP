if(VULKAN_BACKEND)
    add_library(vk_renderer STATIC
        vk_init.c
        vk_renderer.c
        vk_renderer_draw.c
        vk_material.c
        vk_surface.c
        vk_math.c
        vk_sky.c
    )

    target_include_directories(vk_renderer PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../qcommon
        ${CMAKE_CURRENT_SOURCE_DIR}/../renderer
        ${CMAKE_CURRENT_SOURCE_DIR}/../game
    )

    find_package(Vulkan REQUIRED)
    target_link_libraries(vk_renderer PUBLIC Vulkan::Vulkan)

    target_compile_definitions(vk_renderer PRIVATE VULKAN_BACKEND)

    set(VK_RENDERER_LIB vk_renderer PARENT_SCOPE)
endif()
