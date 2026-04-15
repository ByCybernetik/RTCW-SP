# RTCW-SP CMake build system with SDL2
# Supports Linux x86 and x64

cmake_minimum_required(VERSION 3.20)
project(RTCW-SP VERSION 1.41 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(ARCH "x86_64")
    set(ARCH_X64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i.86|x86|X86")
    set(ARCH "x86")
    set(ARCH_X86 TRUE)
else()
    set(ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "Building for architecture: ${ARCH}")

# Platform detection
if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

# Use SDL2 for OpenGL function loading
add_definitions(-DSDL2_QGL)

# Find SDL2 using pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
find_package(OpenGL REQUIRED)
find_package(ALSA)

# Add SDL2 flags
add_compile_options(${SDL2_CFLAGS_OTHER})
link_directories(${SDL2_LIBRARY_DIRS})

# Compiler flags
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_C_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        -pipe
        -fsigned-char
        -Wall
        -Wno-unknown-pragmas
        -Wno-write-strings
        -Wno-deprecated-declarations
    )
    
    if(ARCH_X86)
        # x86 specific flags
        add_compile_options(-m32)
        add_link_options(-m32)
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        # -O0 для совместимости с x64
        add_compile_options(-O0)
        if(ARCH_X86)
            add_compile_options(-march=pentium)
        endif()
        add_definitions(-DNDEBUG)
    else()
        add_compile_options(-g -O0)
    endif()
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/qcommon
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/server
    ${CMAKE_CURRENT_SOURCE_DIR}/src/unix
    ${CMAKE_CURRENT_SOURCE_DIR}/src/client
    ${CMAKE_CURRENT_SOURCE_DIR}/src/botlib
    ${CMAKE_CURRENT_SOURCE_DIR}/src/splines
    ${CMAKE_CURRENT_SOURCE_DIR}/src/jpeg-6
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIRS}
)

# Common source files
set(GAME_COMMON_SOURCES
    src/game/q_shared.c
    src/game/q_math.c
)

# OpenGL renderer sources
set(RENDERER_SOURCES
        src/renderer/tr_animation.c
        src/renderer/tr_backend.c
        src/renderer/tr_bsp.c
        src/renderer/tr_cmds.c
        src/renderer/tr_cmesh.c
        src/renderer/tr_curve.c
        src/renderer/tr_flares.c
        src/renderer/tr_font.c
        src/renderer/tr_image.c
        src/renderer/tr_init.c
        src/renderer/tr_light.c
        src/renderer/tr_main.c
        src/renderer/tr_marks.c
        src/renderer/tr_mesh.c
        src/renderer/tr_model.c
        src/renderer/tr_noise.c
        src/renderer/tr_scene.c
        src/renderer/tr_shade.c
        src/renderer/tr_shade_calc.c
        src/renderer/tr_shader.c
        src/renderer/tr_shadows.c
        src/renderer/tr_sky.c
        src/renderer/tr_surface.c
        src/renderer/tr_world.c
)

# Client sources
set(CLIENT_SOURCES
    src/client/cl_cgame.c
    src/client/cl_cin.c
    src/client/cl_console.c
    src/client/cl_input.c
    src/client/cl_keys.c
    src/client/cl_main.c
    src/client/cl_net_chan.c
    src/client/cl_parse.c
    src/client/cl_scrn.c
    src/client/cl_ui.c
    src/client/snd_adpcm.c
    src/client/snd_dma.c
    src/client/snd_mem.c
    src/client/snd_mix.c
    src/client/snd_wavelet.c
)

# Server sources
set(SERVER_SOURCES
    src/server/sv_bot.c
    src/server/sv_ccmds.c
    src/server/sv_client.c
    src/server/sv_game.c
    src/server/sv_init.c
    src/server/sv_main.c
    src/server/sv_net_chan.c
    src/server/sv_snapshot.c
    src/server/sv_world.c
)

# Renderer library - OpenGL renderer as separate shared library
set(RENDERER_LIB_SOURCES
    ${RENDERER_SOURCES}
    src/unix/sdl2_qgl.c
)

add_library(renderer SHARED ${RENDERER_LIB_SOURCES})
target_compile_definitions(renderer PRIVATE -DSDL2_QGL)
target_compile_options(renderer PRIVATE -fPIC -O0)
target_link_libraries(renderer
    ${OPENGL_LIBRARIES}
    ${SDL2_LIBRARIES}
    m
    dl
)
set_target_properties(renderer PROPERTIES
    PREFIX ""
    OUTPUT_NAME "renderer${ARCH}"
    SUFFIX ".so"
)

# Common sources
set(COMMON_SOURCES
    src/qcommon/cm_load.c
    src/qcommon/cm_patch.c
    src/qcommon/cm_polylib.c
    src/qcommon/cm_test.c
    src/qcommon/cm_trace.c
    src/qcommon/cmd.c
    src/qcommon/common.c
    src/qcommon/cvar.c
    src/qcommon/files.c
    src/qcommon/md4.c
    src/qcommon/msg.c
    src/qcommon/net_chan.c
    src/qcommon/huffman.c
    src/qcommon/unzip.c
)

# VM sources
set(VM_SOURCES
    src/qcommon/vm.c
    src/qcommon/vm_interpreted.c
)

# Only include x86 JIT compiler on x86
if(ARCH_X86)
    list(APPEND VM_SOURCES src/qcommon/vm_x86.c)
endif()

# Unix/SDL2 platform sources
set(UNIX_SOURCES
    src/unix/unix_main.c
    src/unix/unix_net.c
    src/unix/unix_shared.c
    src/unix/unix_sdl2.c
    src/unix/sdl2_qgl.c
)

# Botlib sources
set(BOTLIB_SOURCES
    src/botlib/be_aas_bspq3.c
    src/botlib/be_aas_cluster.c
    src/botlib/be_aas_debug.c
    src/botlib/be_aas_entity.c
    src/botlib/be_aas_file.c
    src/botlib/be_aas_main.c
    src/botlib/be_aas_move.c
    src/botlib/be_aas_optimize.c
    src/botlib/be_aas_reach.c
    src/botlib/be_aas_route.c
    src/botlib/be_aas_routealt.c
    src/botlib/be_aas_routetable.c
    src/botlib/be_aas_sample.c
    src/botlib/be_ai_char.c
    src/botlib/be_ai_chat.c
    src/botlib/be_ai_gen.c
    src/botlib/be_ai_goal.c
    src/botlib/be_ai_move.c
    src/botlib/be_ai_weap.c
    src/botlib/be_ai_weight.c
    src/botlib/be_ea.c
    src/botlib/be_interface.c
    src/botlib/l_crc.c
    src/botlib/l_libvar.c
    src/botlib/l_log.c
    src/botlib/l_memory.c
    src/botlib/l_precomp.c
    src/botlib/l_script.c
    src/botlib/l_struct.c
)

# Botlib needs BOTLIB define
set_source_files_properties(${BOTLIB_SOURCES} PROPERTIES COMPILE_DEFINITIONS "BOTLIB")

# JPEG library
set(JPEG_SOURCES
    src/jpeg-6/jcapimin.c
    src/jpeg-6/jchuff.c
    src/jpeg-6/jcinit.c
    src/jpeg-6/jccoefct.c
    src/jpeg-6/jccolor.c
    src/jpeg-6/jfdctflt.c
    src/jpeg-6/jcdctmgr.c
    src/jpeg-6/jcphuff.c
    src/jpeg-6/jcmainct.c
    src/jpeg-6/jcmarker.c
    src/jpeg-6/jcmaster.c
    src/jpeg-6/jcomapi.c
    src/jpeg-6/jcparam.c
    src/jpeg-6/jcprepct.c
    src/jpeg-6/jcsample.c
    src/jpeg-6/jdapimin.c
    src/jpeg-6/jdapistd.c
    src/jpeg-6/jdatasrc.c
    src/jpeg-6/jdcoefct.c
    src/jpeg-6/jdcolor.c
    src/jpeg-6/jddctmgr.c
    src/jpeg-6/jdhuff.c
    src/jpeg-6/jdinput.c
    src/jpeg-6/jdmainct.c
    src/jpeg-6/jdmarker.c
    src/jpeg-6/jdmaster.c
    src/jpeg-6/jdpostct.c
    src/jpeg-6/jdsample.c
    src/jpeg-6/jdtrans.c
    src/jpeg-6/jerror.c
    src/jpeg-6/jidctflt.c
    src/jpeg-6/jmemmgr.c
    src/jpeg-6/jmemnobs.c
    src/jpeg-6/jutils.c
)

# Splines library
set(SPLINES_SOURCES
    src/splines/math_angles.cpp
    src/splines/math_matrix.cpp
    src/splines/math_quaternion.cpp
    src/splines/math_vector.cpp
    src/splines/q_parse.cpp
    src/splines/splines.cpp
    src/splines/util_str.cpp
)

# Platform libraries
set(PLATFORM_LIBS
    ${SDL2_LIBRARIES}
    OpenGL::GL
    m
    dl
    pthread
)

if(ALSA_FOUND)
    include_directories(${ALSA_INCLUDE_DIRS})
    list(APPEND PLATFORM_LIBS ${ALSA_LIBRARIES})
    add_definitions(-DHAVE_ALSA)
endif()

# Main executable
add_executable(wolfsp.${ARCH}
    ${GAME_COMMON_SOURCES}
    ${RENDERER_SOURCES}
    ${CLIENT_SOURCES}
    ${SERVER_SOURCES}
    ${COMMON_SOURCES}
    ${VM_SOURCES}
    ${UNIX_SOURCES}
    ${BOTLIB_SOURCES}
    ${JPEG_SOURCES}
    ${SPLINES_SOURCES}
)

target_link_libraries(wolfsp.${ARCH}
    ${PLATFORM_LIBS}
)

# Game library (qagame)
set(GAME_SOURCES
    src/game/ai_cast.c
    src/game/ai_cast_characters.c
    src/game/ai_cast_debug.c
    src/game/ai_cast_events.c
    src/game/ai_cast_fight.c
    src/game/ai_cast_func_attack.c
    src/game/ai_cast_func_boss1.c
    src/game/ai_cast_funcs.c
    src/game/ai_cast_script.c
    src/game/ai_cast_script_actions.c
    src/game/ai_cast_script_ents.c
    src/game/ai_cast_sight.c
    src/game/ai_cast_think.c
    src/botai/ai_chat.c
    src/botai/ai_cmd.c
    src/botai/ai_dmnet.c
    src/botai/ai_dmq3.c
    src/botai/ai_main.c
    src/botai/ai_team.c
    src/game/bg_animation.c
    src/game/bg_misc.c
    src/game/bg_pmove.c
    src/game/bg_slidemove.c
    src/game/g_active.c
    src/game/g_alarm.c
    src/game/g_bot.c
    src/game/g_client.c
    src/game/g_cmds.c
    src/game/g_combat.c
    src/game/g_items.c
    src/game/g_main.c
    src/game/g_mem.c
    src/game/g_misc.c
    src/game/g_missile.c
    src/game/g_mover.c
    src/game/g_props.c
    src/game/g_save.c
    src/game/g_script.c
    src/game/g_script_actions.c
    src/game/g_session.c
    src/game/g_spawn.c
    src/game/g_svcmds.c
    src/game/g_syscalls.c
    src/game/g_target.c
    src/game/g_team.c
    src/game/g_tramcar.c
    src/game/g_trigger.c
    src/game/g_utils.c
    src/game/g_weapon.c
    src/game/q_math.c
    src/game/q_shared.c
)

add_library(qagame SHARED ${GAME_SOURCES})
target_compile_definitions(qagame PRIVATE GAMEDLL)
target_compile_options(qagame PRIVATE -fPIC -O0)
set_target_properties(qagame PROPERTIES
    PREFIX ""
    OUTPUT_NAME "qagame${ARCH}"
    SUFFIX ".so"
)

# CGame library
set(CGAME_SOURCES
    src/game/bg_animation.c
    src/game/bg_misc.c
    src/game/bg_pmove.c
    src/game/bg_slidemove.c
    src/cgame/cg_consolecmds.c
    src/cgame/cg_draw.c
    src/cgame/cg_drawtools.c
    src/cgame/cg_effects.c
    src/cgame/cg_ents.c
    src/cgame/cg_event.c
    src/cgame/cg_flamethrower.c
    src/cgame/cg_info.c
    src/cgame/cg_localents.c
    src/cgame/cg_main.c
    src/cgame/cg_marks.c
    src/cgame/cg_newDraw.c
    src/cgame/cg_particles.c
    src/cgame/cg_players.c
    src/cgame/cg_playerstate.c
    src/cgame/cg_predict.c
    src/cgame/cg_scoreboard.c
    src/cgame/cg_servercmds.c
    src/cgame/cg_snapshot.c
    src/cgame/cg_sound.c
    src/cgame/cg_syscalls.c
    src/cgame/cg_trails.c
    src/cgame/cg_view.c
    src/cgame/cg_weapons.c
    src/game/q_math.c
    src/game/q_shared.c
    src/ui/ui_shared.c
)

add_library(cgame SHARED ${CGAME_SOURCES})
target_compile_definitions(cgame PRIVATE CGAMEDLL)
target_compile_options(cgame PRIVATE -fPIC -O0)
set_target_properties(cgame PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cgame${ARCH}"
    SUFFIX ".so"
)

# UI library
set(UI_SOURCES
    src/game/bg_misc.c
    src/game/q_math.c
    src/game/q_shared.c
    src/ui/ui_atoms.c
    src/ui/ui_gameinfo.c
    src/ui/ui_main.c
    src/ui/ui_players.c
    src/ui/ui_shared.c
    src/ui/ui_syscalls.c
    src/ui/ui_util.c
)

add_library(ui SHARED ${UI_SOURCES})
target_compile_options(ui PRIVATE -fPIC -O0)
set_target_properties(ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "ui${ARCH}"
    SUFFIX ".so"
)

# Create main directory in build folder
set(MAIN_DIR "${CMAKE_BINARY_DIR}/main")
file(MAKE_DIRECTORY ${MAIN_DIR})

# Copy .so files to main directory after building
add_custom_command(TARGET qagame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:qagame> ${MAIN_DIR}/
    COMMENT "Copying qagame.so to main/"
)

add_custom_command(TARGET cgame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:cgame> ${MAIN_DIR}/
    COMMENT "Copying cgame.so to main/"
)

add_custom_command(TARGET ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:ui> ${MAIN_DIR}/
    COMMENT "Copying ui.so to main/"
)

add_custom_command(TARGET renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:renderer> ${MAIN_DIR}/
    COMMENT "Copying renderer.so to main/"
)

# Installation
install(TARGETS wolfsp.${ARCH} qagame cgame ui renderer
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

# Print configuration summary
message(STATUS "")
message(STATUS "RTCW-SP Build Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Architecture: ${ARCH}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  SDL2: ${SDL2_LIBRARIES}")
message(STATUS "  Renderer: OpenGL")
message(STATUS "")
