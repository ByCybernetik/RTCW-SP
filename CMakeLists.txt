cmake_minimum_required(VERSION 3.16)
project(RTCW VERSION 1.41 LANGUAGES C CXX)

# Стандарты компиляции
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Позиционно-независимый код для x64
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Предупреждения компилятора
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable)
    # Отключаем предупреждения о приведении типов указателей
    add_compile_options(-Wno-int-to-void-pointer-cast -Wno-void-pointer-to-int-cast)
endif()

# Находим зависимости SDL2
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# Определяем платформу
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_UNIX 1)
    set(PLATFORM_LINUX 1)
    add_definitions(-D__linux__)
    add_definitions(-DLINUX)
endif()

# Определяем архитектуру
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH_X64 1)
    add_definitions(-DARCH_X64)
    message(STATUS "Building for x64 architecture")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(ARCH_X86 1)
    add_definitions(-DARCH_X86)
    message(STATUS "Building for x86 architecture")
endif()

# Пути к исходникам
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(COMMON_DIR ${SRC_DIR}/qcommon)
set(RENDERER_DIR ${SRC_DIR}/renderer)
set(CLIENT_DIR ${SRC_DIR}/client)
set(GAME_DIR ${SRC_DIR}/game)
set(CGAME_DIR ${SRC_DIR}/cgame)
set(UI_DIR ${SRC_DIR}/ui)
set(SDL_DIR ${SRC_DIR}/sdl2)
set(MATH_DIR ${SRC_DIR}/math)

# Общие заголовки
include_directories(
    ${SRC_DIR}
    ${SRC_DIR}/game
    ${SRC_DIR}/qcommon
    ${SRC_DIR}/renderer
    ${SRC_DIR}/client
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

# Исходники общих модулей (qcommon)
set(QCOMMON_SOURCES
    ${COMMON_DIR}/common.c
    ${COMMON_DIR}/cm_load.c
    ${COMMON_DIR}/cm_test.c
    ${COMMON_DIR}/cm_trace.c
    ${COMMON_DIR}/cm_patch.c
    ${COMMON_DIR}/cm_polylib.c
    ${COMMON_DIR}/cmd.c
    ${COMMON_DIR}/msg.c
    ${COMMON_DIR}/files.c
    ${COMMON_DIR}/cvar.c
    ${COMMON_DIR}/net_chan.c
    ${COMMON_DIR}/huffman.c
    ${COMMON_DIR}/md4.c
    ${COMMON_DIR}/vm.c
    # Native x64 VM implementation (replaces vm_x86.c and vm_interpreted.c)
    ${COMMON_DIR}/vm_x64.c
    ${COMMON_DIR}/sys_x64.c
)

# Assembly files for x64 VM helpers
if(ARCH_X64)
    set(QCOMMON_SOURCES ${QCOMMON_SOURCES} ${COMMON_DIR}/vm_x64_asm.S)
endif()

# Исходники рендерера
set(RENDERER_SOURCES
    ${RENDERER_DIR}/tr_init.c
    ${RENDERER_DIR}/tr_backend.c
    ${RENDERER_DIR}/tr_bsp.c
    ${RENDERER_DIR}/tr_cmds.c
    ${RENDERER_DIR}/tr_curve.c
    ${RENDERER_DIR}/tr_image.c
    ${RENDERER_DIR}/tr_light.c
    ${RENDERER_DIR}/tr_main.c
    ${RENDERER_DIR}/tr_marks.c
    ${RENDERER_DIR}/tr_mesh.c
    ${RENDERER_DIR}/tr_model.c
    ${RENDERER_DIR}/tr_scene.c
    ${RENDERER_DIR}/tr_shade.c
    ${RENDERER_DIR}/tr_shade_calc.c
    ${RENDERER_DIR}/tr_shader.c
    ${RENDERER_DIR}/tr_sky.c
    ${RENDERER_DIR}/tr_surface.c
    ${RENDERER_DIR}/tr_world.c
)

# Исходники клиента
set(CLIENT_SOURCES
    ${CLIENT_DIR}/cl_main.c
    ${CLIENT_DIR}/cl_input.c
    ${CLIENT_DIR}/cl_parse.c
    ${CLIENT_DIR}/cl_scrn.c
    ${CLIENT_DIR}/cl_cin.c
    ${CLIENT_DIR}/cl_keys.c
    ${CLIENT_DIR}/cl_console.c
    ${CLIENT_DIR}/cl_cgame.c
    ${CLIENT_DIR}/cl_ui.c
    ${CLIENT_DIR}/cl_net_chan.c
    ${CLIENT_DIR}/snd_dma.c
    ${CLIENT_DIR}/snd_mem.c
    ${CLIENT_DIR}/snd_mix.c
    ${CLIENT_DIR}/snd_wavelet.c
    ${CLIENT_DIR}/snd_adpcm.c
)

# Исходники SDL2 платформы
set(SDL_PLATFORM_SOURCES
    ${SDL_DIR}/sdl_glimp.c      # OpenGL контекст через SDL2
    ${SDL_DIR}/sdl_input.c      # Ввод (клавиатура, мышь) через SDL2
    ${SDL_DIR}/sdl_snd.c        # Звук через SDL2
    ${SDL_DIR}/sdl_main.c       # Точка входа и основной цикл
    ${SDL_DIR}/sdl_shared.c     # Общие функции платформы
)

# Объединяем все исходники клиента
set(CLIENT_ALL_SOURCES
    ${QCOMMON_SOURCES}
    ${RENDERER_SOURCES}
    ${CLIENT_SOURCES}
    ${SDL_PLATFORM_SOURCES}
)

# Создаем исполняемый файл клиента
add_executable(rtcw_client ${CLIENT_ALL_SOURCES})

# Линкуем библиотеки
target_link_libraries(rtcw_client
    ${SDL2_LIBRARIES}
    ${OPENGL_gl_LIBRARY}
    m  # math library для Linux
    dl # dynamic loading
    pthread
)

# Добавляем определения для клиента
target_compile_definitions(rtcw_client PRIVATE
    CLIENT
    USE_SDL2
    ARCH_${CMAKE_SIZEOF_VOID_P}
)

# Установка правильных типов для x64
target_compile_definitions(rtcw_client PRIVATE
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:PTR_64>
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:PTR_32>
)

# Игровая библиотека (game)
set(GAME_SOURCES
    ${GAME_DIR}/g_main.c
    ${GAME_DIR}/g_spawn.c
    ${GAME_DIR}/g_utils.c
    ${GAME_DIR}/g_weapon.c
    ${GAME_DIR}/g_combat.c
    ${GAME_DIR}/g_misc.c
    ${GAME_DIR}/g_cmds.c
    ${GAME_DIR}/g_svcmds.c
    ${GAME_DIR}/g_team.c
    ${GAME_DIR}/g_active.c
    ${GAME_DIR}/g_alarm.c
    ${GAME_DIR}/g_bot.c
    ${GAME_DIR}/g_client.c
    ${GAME_DIR}/g_items.c
    ${GAME_DIR}/g_mem.c
    ${GAME_DIR}/g_missile.c
    ${GAME_DIR}/g_mover.c
    ${GAME_DIR}/g_props.c
    ${GAME_DIR}/g_save.c
    ${GAME_DIR}/g_script.c
    ${GAME_DIR}/g_script_actions.c
    ${GAME_DIR}/g_session.c
    ${GAME_DIR}/g_syscalls.c
    ${GAME_DIR}/g_target.c
    ${GAME_DIR}/g_trigger.c
    ${GAME_DIR}/g_tramcar.c
    ${GAME_DIR}/ai_cast.c
    ${GAME_DIR}/ai_cast_characters.c
    ${GAME_DIR}/ai_cast_debug.c
    ${GAME_DIR}/ai_cast_events.c
    ${GAME_DIR}/ai_cast_fight.c
    ${GAME_DIR}/ai_cast_func_attack.c
    ${GAME_DIR}/ai_cast_func_boss1.c
    ${GAME_DIR}/ai_cast_funcs.c
    ${GAME_DIR}/ai_cast_script.c
    ${GAME_DIR}/ai_cast_script_actions.c
    ${GAME_DIR}/ai_cast_script_ents.c
    ${GAME_DIR}/ai_cast_sight.c
    ${GAME_DIR}/ai_cast_think.c
    ${GAME_DIR}/bg_animation.c
    ${GAME_DIR}/bg_misc.c
    ${GAME_DIR}/bg_pmove.c
    ${GAME_DIR}/bg_slidemove.c
    ${GAME_DIR}/bg_lib.c
    ${GAME_DIR}/q_math.c
    ${GAME_DIR}/q_shared.c
)

add_library(game SHARED ${GAME_SOURCES})

target_include_directories(game PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/game
)

target_compile_definitions(game PRIVATE
    GAME
)

# CGAME библиотека
set(CGAME_SOURCES
    ${CGAME_DIR}/cg_main.c
    ${CGAME_DIR}/cg_draw.c
    ${CGAME_DIR}/cg_weapons.c
    ${CGAME_DIR}/cg_players.c
    ${CGAME_DIR}/cg_playerstate.c
    ${CGAME_DIR}/cg_predict.c
    ${CGAME_DIR}/cg_ents.c
    ${CGAME_DIR}/cg_effects.c
    ${CGAME_DIR}/cg_localents.c
    ${CGAME_DIR}/cg_marks.c
    ${CGAME_DIR}/cg_particles.c
    ${CGAME_DIR}/cg_view.c
    ${CGAME_DIR}/cg_servercmds.c
    ${CGAME_DIR}/cg_info.c
    ${CGAME_DIR}/cg_consolecmds.c
    ${CGAME_DIR}/cg_drawtools.c
    ${CGAME_DIR}/cg_event.c
    ${CGAME_DIR}/cg_flamethrower.c
    ${CGAME_DIR}/cg_newDraw.c
    ${CGAME_DIR}/cg_scoreboard.c
    ${CGAME_DIR}/cg_snapshot.c
    ${CGAME_DIR}/cg_sound.c
    ${CGAME_DIR}/cg_syscalls.c
    ${CGAME_DIR}/cg_trails.c
    ${GAME_DIR}/bg_misc.c
    ${GAME_DIR}/bg_pmove.c
    ${GAME_DIR}/bg_slidemove.c
    ${GAME_DIR}/q_math.c
)

add_library(cgame SHARED ${CGAME_SOURCES})

target_include_directories(cgame PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/cgame
    ${SRC_DIR}/game
)

target_compile_definitions(cgame PRIVATE
    CGAME
)

# UI библиотека
set(UI_SOURCES
    ${UI_DIR}/ui_main.c
    ${UI_DIR}/ui_atoms.c
    ${UI_DIR}/ui_shared.c
    ${UI_DIR}/ui_gameinfo.c
    ${UI_DIR}/ui_players.c
    ${UI_DIR}/ui_syscalls.c
    ${UI_DIR}/ui_util.c
)

add_library(ui SHARED ${UI_SOURCES})

target_include_directories(ui PRIVATE
    ${SRC_DIR}
    ${SRC_DIR}/ui
    ${SRC_DIR}/game
)

target_compile_definitions(ui PRIVATE
    UI
)

# Правила установки
install(TARGETS rtcw_client game cgame ui
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib/rtcw
)

# Сообщение о конфигурации
message(STATUS "===========================================")
message(STATUS "RTCW Build Configuration:")
message(STATUS "  Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Architecture: ${CMAKE_SIZEOF_VOID_P}-bit")
message(STATUS "  SDL2 Version: ${SDL2_VERSION}")
message(STATUS "  OpenGL: ${OPENGL_gl_LIBRARY}")
message(STATUS "===========================================")
