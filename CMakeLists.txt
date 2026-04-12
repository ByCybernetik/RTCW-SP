# Return to Castle Wolfenstein - Modern Arch Linux x64 Port
# CMake build system with SDL2 and native .so modules (no QVM)

cmake_minimum_required(VERSION 3.16)
project(iortcw-x64 VERSION 1.51 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build options
option(BUILD_CLIENT "Build client executable" ON)
option(BUILD_SERVER "Build server executable" ON)
option(BUILD_GAME_MODULE "Build game module as .so" ON)
option(BUILD_CGAME_MODULE "Build cgame module as .so" ON)
option(BUILD_UI_MODULE "Build ui module as .so" ON)
option(USE_SDL2 "Use SDL2 for input, audio and window" ON)
option(USE_OPENAL "Use OpenAL for audio" ON)
option(USE_OPENGL "Use OpenGL for rendering" ON)

# Compiler flags for x64
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-unused-function")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -m64")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_C_FLAGS_DEBUG "-g -DDEBUG")

# Find required packages
if(USE_SDL2)
    find_package(SDL2 REQUIRED)
    find_package(SDL2_image QUIET)
endif()

if(USE_OPENAL)
    find_package(OpenAL REQUIRED)
endif()

if(USE_OPENGL)
    find_package(OpenGL REQUIRED)
    find_package(GLEW QUIET)
endif()

find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JPEG REQUIRED libjpeg)
pkg_check_modules(PNG REQUIRED libpng)
find_package(Freetype QUIET)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/game
    ${CMAKE_SOURCE_DIR}/src/cgame
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/qcommon
    ${CMAKE_SOURCE_DIR}/src/client
    ${CMAKE_SOURCE_DIR}/src/renderer
    ${CMAKE_SOURCE_DIR}/src/server
    ${CMAKE_SOURCE_DIR}/src/botlib
    ${CMAKE_SOURCE_DIR}/src/splines
    ${CMAKE_SOURCE_DIR}/src/idLib
    ${SDL2_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIR}
    ${OPENGL_INCLUDE_DIR}
    ${JPEG_INCLUDE_DIRS}
    ${PNG_INCLUDE_DIRS}
)

# Common source files
set(COMMON_SOURCES
    src/qcommon/common.c
    src/qcommon/cmd.c
    src/qcommon/cvar.c
    src/qcommon/files.c
    src/qcommon/huffman.c
    src/qcommon/md4.c
    src/qcommon/msg.c
    src/qcommon/net_chan.c
    src/qcommon/unzip.c
    src/qcommon/cm_load.c
    src/qcommon/cm_patch.c
    src/qcommon/cm_polylib.c
    src/qcommon/cm_test.c
    src/qcommon/cm_trace.c
    src/splines/math_vector.cpp
    src/splines/math_matrix.cpp
    src/splines/math_quaternion.cpp
    src/splines/math_angles.cpp
    src/splines/splines.cpp
    src/splines/q_parse.cpp
    src/splines/q_shared.cpp
    src/splines/util_str.cpp
    src/idLib/idQ3Asnd.cpp
    src/idLib/idSound.cpp
    src/idLib/idSpeaker.cpp
)

# Game module sources (compiled to .so instead of QVM)
set(GAME_SOURCES
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
    src/game/bg_animation.c
    src/game/bg_lib.c
    src/game/bg_misc.c
    src/game/bg_pmove.c
    src/game/bg_slidemove.c
    src/game/q_math.c
    src/game/q_shared.c
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
)

# CGAME module sources (compiled to .so instead of QVM)
set(CGAME_SOURCES
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
    src/cgame/cg_trails.c
    src/cgame/cg_view.c
    src/cgame/cg_weapons.c
    src/game/bg_animation.c
    src/game/bg_lib.c
    src/game/bg_misc.c
    src/game/bg_pmove.c
    src/game/bg_slidemove.c
    src/game/q_math.c
    src/game/q_shared.c
)

# UI module sources (compiled to .so instead of QVM)
set(UI_SOURCES
    src/ui/ui_atoms.c
    src/ui/ui_gameinfo.c
    src/ui/ui_main.c
    src/ui/ui_players.c
    src/ui/ui_shared.c
    src/ui/ui_syscalls.c
    src/ui/ui_util.c
    src/game/bg_lib.c
    src/game/q_math.c
    src/game/q_shared.c
)

# Botlib sources
set(BOTLIB_SOURCES
    src/botlib/be_aas_entity.c
    src/botlib/be_aas_file.c
    src/botlib/be_aas_move.c
    src/botlib/be_aas_optimize.c
    src/botlib/be_aas_reach.c
    src/botlib/be_aas_route.c
    src/botlib/be_aas_routealt.c
    src/botlib/be_aas_sample.c
    src/botlib/be_ai_char.c
    src/botlib/be_ai_chat.c
    src/botlib/be_ai_gen.c
    src/botlib/be_ai_goal.c
    src/botlib/be_ai_move.c
    src/botlib/be_ai_weap.c
    src/botlib/be_ea.c
    src/botlib/l_crc.c
    src/botlib/l_libvar.c
    src/botlib/l_log.c
    src/botlib/l_memory.c
    src/botlib/l_precomp.c
    src/botlib/l_script.c
    src/botlib/l_struct.c
    src/botlib/be_interface.c
    src/botlib/be_aas_bspq3.c
    src/botlib/be_aas_cluster.c
    src/botlib/be_aas_debug.c
    src/botlib/be_aas_main.c
    src/botlib/be_aas_routetable.c
    src/botlib/be_ai_weight.c
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

# Unix/Linux specific sources
set(UNIX_SOURCES
    src/unix/linux_common.c
    src/unix/linux_glimp.c
    src/unix/linux_qgl.c
    src/unix/linux_snd.c
    src/unix/linux_joystick.c
    src/unix/unix_main.c
    src/unix/unix_net.c
    src/unix/unix_shared.c
)

# Renderer sources
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

# Build game module as native .so library
if(BUILD_GAME_MODULE)
    add_library(game MODULE ${GAME_SOURCES} ${BOTLIB_SOURCES})
    target_compile_definitions(game PRIVATE GAME_LIBRARY)
    set_target_properties(game PROPERTIES 
        PREFIX "" 
        SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_link_libraries(game m)
endif()

# Build cgame module as native .so library
if(BUILD_CGAME_MODULE)
    add_library(cgame MODULE ${CGAME_SOURCES})
    target_compile_definitions(cgame PRIVATE CGAME_LIBRARY)
    set_target_properties(cgame PROPERTIES 
        PREFIX "" 
        SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_link_libraries(cgame m)
endif()

# Build ui module as native .so library
if(BUILD_UI_MODULE)
    add_library(ui MODULE ${UI_SOURCES})
    target_compile_definitions(ui PRIVATE UI_LIBRARY)
    set_target_properties(ui PROPERTIES 
        PREFIX "" 
        SUFFIX ".so"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_link_libraries(ui m)
endif()

# Build client executable
if(BUILD_CLIENT AND USE_SDL2)
    add_executable(wolf.x64 
        ${COMMON_SOURCES}
        ${CLIENT_SOURCES}
        ${UNIX_SOURCES}
        ${RENDERER_SOURCES}
    )
    
    target_compile_definitions(wolf.x64 PRIVATE CLIENT_BUILD BUILD_STANDALONE)
    
    target_link_libraries(wolf.x64
        ${SDL2_LIBRARIES}
        ${OPENAL_LIBRARY}
        ${OPENGL_LIBRARIES}
        ${JPEG_LIBRARIES}
        ${PNG_LIBRARIES}
        ZLIB::ZLIB
        m
        pthread
        dl
    )
    
    if(Freetype_FOUND)
        target_link_libraries(wolf.x64 Freetype::Freetype)
        target_compile_definitions(wolf.x64 PRIVATE USE_FREETYPE)
    endif()
    
    set_target_properties(wolf.x64 PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# Build dedicated server executable
if(BUILD_SERVER)
    add_executable(wolfded.x64 
        ${COMMON_SOURCES}
        ${SERVER_SOURCES}
        src/unix/unix_main.c
        src/unix/unix_net.c
        src/unix/unix_shared.c
        src/unix/linux_common.c
    )
    
    target_compile_definitions(wolfded.x64 PRIVATE SERVER_BUILD BUILD_STANDALONE)
    
    target_link_libraries(wolfded.x64
        ${JPEG_LIBRARIES}
        ${PNG_LIBRARIES}
        ZLIB::ZLIB
        m
        pthread
        dl
    )
    
    set_target_properties(wolfded.x64 PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# Installation
install(TARGETS wolf.x64 wolfded.x64 game cgame ui
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
)

# Configure header with build info
configure_file(
    ${CMAKE_SOURCE_DIR}/src/unix/buildinfo.h.in
    ${CMAKE_BINARY_DIR}/buildinfo.h
    @ONLY
)

message(STATUS "=== RTCW x64 Build Configuration ===")
message(STATUS "Build Client: ${BUILD_CLIENT}")
message(STATUS "Build Server: ${BUILD_SERVER}")
message(STATUS "Build Game Module: ${BUILD_GAME_MODULE}")
message(STATUS "Build CGAME Module: ${BUILD_CGAME_MODULE}")
message(STATUS "Build UI Module: ${BUILD_UI_MODULE}")
message(STATUS "SDL2: ${SDL2_LIBRARIES}")
message(STATUS "OpenAL: ${OPENAL_LIBRARY}")
message(STATUS "OpenGL: ${OPENGL_LIBRARIES}")
message(STATUS "=====================================")
