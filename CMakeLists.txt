cmake_minimum_required(VERSION 3.16)
project(RTCW_SP VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Опции сборки
option(BUILD_CLIENT "Build client executable" ON)
option(BUILD_SERVER "Build server executable" OFF)
option(BUILD_GAME_DLL "Build game DLL" ON)
option(BUILD_CGAME_DLL "Build cgame DLL" ON)
option(BUILD_UI_DLL "Build ui DLL" ON)

# Поиск зависимостей
find_package(SDL2 REQUIRED)
find_package(OpenAL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FREETYPE2 REQUIRED freetype2)

# Платформенно-зависимые настройки (SDL2-based)
if(WIN32)
    add_definitions(-DWIN32 -D_WIN32)
    set(PLATFORM_SRC 
        src/unix/sys_sdl_main.c
        src/unix/sys_sdl_input.c
        src/unix/sys_sdl_sound.c
        src/unix/sys_sdl_dll.c
        src/unix/sys_sdl_gl.c
    )
    set(PLATFORM_LIBS ws2_32 opengl32)
elseif(APPLE)
    add_definitions(-DMACOS_X)
    set(PLATFORM_SRC 
        src/unix/sys_sdl_main.c
        src/unix/sys_sdl_input.c
        src/unix/sys_sdl_sound.c
        src/unix/sys_sdl_dll.c
        src/unix/sys_sdl_gl.c
    )
    set(PLATFORM_LIBS 
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
else()
    add_definitions(-DLINUX -D__linux__)
    set(PLATFORM_SRC 
        src/unix/sys_sdl_main.c
        src/unix/sys_sdl_input.c
        src/unix/sys_sdl_sound.c
        src/unix/sys_sdl_dll.c
        src/unix/sys_sdl_gl.c
    )
    set(PLATFORM_LIBS dl m pthread)
endif()

# Общие исходники qcommon
set(QCOMMON_SRC
    src/qcommon/cmd.c
    src/qcommon/common.c
    src/qcommon/cvar.c
    src/qcommon/files.c
    src/qcommon/huffman.c
    src/qcommon/md4.c
    src/qcommon/msg.c
    src/qcommon/net_chan.c
    src/qcommon/cm_load.c
    src/qcommon/cm_trace.c
    src/qcommon/cm_test.c
    src/qcommon/cm_patch.c
    src/qcommon/cm_polylib.c
    src/qcommon/unzip.c
)

# Исходники клиента
set(CLIENT_SRC
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

# Исходники рендерера
set(RENDERER_SRC
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

# Библиотеки
set(LIBJPEG_SRC
    src/jpeg-6/jcapimin.c
    src/jpeg-6/jccoefct.c
    src/jpeg-6/jccolor.c
    src/jpeg-6/jcdctmgr.c
    src/jpeg-6/jchuff.c
    src/jpeg-6/jcinit.c
    src/jpeg-6/jcmainct.c
    src/jpeg-6/jcmarker.c
    src/jpeg-6/jcmaster.c
    src/jpeg-6/jcomapi.c
    src/jpeg-6/jcparam.c
    src/jpeg-6/jcphuff.c
    src/jpeg-6/jcprepct.c
    src/jpeg-6/jcsample.c
    src/jpeg-6/jctrans.c
    src/jpeg-6/jdapimin.c
    src/jpeg-6/jdapistd.c
    src/jpeg-6/jdatadst.c
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
    src/jpeg-6/jfdctflt.c
    src/jpeg-6/jidctflt.c
    src/jpeg-6/jmemmgr.c
    src/jpeg-6/jmemnobs.c
    src/jpeg-6/jutils.c
)

# idLib
set(IDLIB_SRC
    src/idLib/idQ3Asnd.cpp
    src/idLib/idSound.cpp
    src/idLib/idSpeaker.cpp
)

# Splines
set(SPLINES_SRC
    src/splines/math_angles.cpp
    src/splines/math_matrix.cpp
    src/splines/math_quaternion.cpp
    src/splines/math_vector.cpp
    src/splines/q_parse.cpp
    src/splines/q_shared.cpp
    src/splines/splines.cpp
    src/splines/util_str.cpp
)

# Bot AI
set(BOTAI_SRC
    src/botai/ai_chat.c
    src/botai/ai_cmd.c
    src/botai/ai_dmq3.c
    src/botai/ai_dmnet.c
    src/botai/ai_main.c
    src/botai/ai_team.c
)

# BotLib
set(BOTLIB_SRC
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

# Общие заголовки
include_directories(
    src
    src/qcommon
    src/client
    src/renderer
    src/game
    src/cgame
    src/ui
    src/botlib
    src/botai
    src/idLib
    src/splines
    src/jpeg-6
    ${SDL2_INCLUDE_DIRS}
    ${OPENAL_INCLUDE_DIR}
    ${FREETYPE2_INCLUDE_DIRS}
)

# Определения компиляции
add_definitions(
    -DRTCW_SP
    -DID_INLINE=inline
    -DCOMPILER_SUPPORTS_SSE
    -DUSE_OPENAL
    -DUSE_SDL
    -DNO_VM_COMPILED
    -Wall
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-misleading-indentation
    -Wno-pointer-sign
    -Wno-format
)

# Настройки для 64-битной сборки
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(-DARCH_X86_64)
    message(STATUS "Building for x64 architecture")
endif()

# Основная библиотека движка (без VM - только нативные DLL)
add_library(engine STATIC
    ${QCOMMON_SRC}
    ${CLIENT_SRC}
    ${RENDERER_SRC}
    ${PLATFORM_SRC}
    ${LIBJPEG_SRC}
    ${IDLIB_SRC}
    ${SPLINES_SRC}
    ${BOTLIB_SRC}
    ${BOTAI_SRC}
    src/ft2/ftinit.c
    src/ft2/ftsystem.c
    src/ft2/ftdebug.c
    src/ft2/ftcalc.c
    src/ft2/ftextend.c
    src/ft2/ftglyph.c
    src/ft2/ftgrays.c
    src/ft2/ftlist.c
    src/ft2/ftmm.c
    src/ft2/ftnames.c
    src/ft2/ftobjs.c
    src/ft2/ftoutln.c
    src/ft2/ftraster.c
    src/ft2/ftrend1.c
    src/ft2/ftsmooth.c
    src/ft2/ftstream.c
    src/ft2/sfdriver.c
    src/ft2/sfobjs.c
    src/ft2/ttcmap.c
    src/ft2/ttdriver.c
    src/ft2/ttgload.c
    src/ft2/ttinterp.c
    src/ft2/ttload.c
    src/ft2/ttobjs.c
    src/ft2/ttpload.c
    src/ft2/ttpost.c
    src/ft2/ttsbit.c
    src/ft2/ahangles.c
    src/ft2/ahglobal.c
    src/ft2/ahglyph.c
    src/ft2/ahhint.c
    src/ft2/ahmodule.c
    src/ft2/ahoptim.c
)

target_compile_definitions(engine PRIVATE
    -DFT2_BUILD_LIBRARY
    -DFT_CONFIG_OPTION_SYSTEM_ZLIB
)

target_link_libraries(engine
    ${SDL2_LIBRARIES}
    ${OPENAL_LIBRARY}
    ${FREETYPE2_LIBRARIES}
    ${PLATFORM_LIBS}
    GL
    z
)

# Game DLL
add_library(game SHARED
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
    src/game/bg_animation.c
    src/game/bg_lib.c
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

target_compile_definitions(game PRIVATE
    -DGAME_DLL
    -DARCH_X86_64
)

target_link_libraries(game engine)
set_target_properties(game PROPERTIES
    PREFIX ""
    OUTPUT_NAME "gamex86_64"
)

# CGame DLL
add_library(cgame SHARED
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
)

target_compile_definitions(cgame PRIVATE
    -DCGAME_DLL
    -DARCH_X86_64
)

target_link_libraries(cgame engine)
set_target_properties(cgame PROPERTIES
    PREFIX ""
    OUTPUT_NAME "cgamex86_64"
)

# UI DLL
add_library(ui SHARED
    src/ui/ui_atoms.c
    src/ui/ui_gameinfo.c
    src/ui/ui_main.c
    src/ui/ui_players.c
    src/ui/ui_shared.c
    src/ui/ui_syscalls.c
    src/ui/ui_util.c
)

target_compile_definitions(ui PRIVATE
    -DUI_DLL
    -DARCH_X86_64
)

target_link_libraries(ui engine)
set_target_properties(ui PROPERTIES
    PREFIX ""
    OUTPUT_NAME "uix86_64"
)

# Клиентский исполняемый файл
if(BUILD_CLIENT)
    add_executable(wolfsp
        src/unix/sys_sdl_main.c
    )
    
    target_link_libraries(wolfsp engine game cgame ui)
    
    if(WIN32)
        set_target_properties(wolfsp PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
    endif()
endif()

# Установка
install(TARGETS wolfsp game cgame ui
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(DIRECTORY main/ DESTINATION share/wolfsp)

# Создаем скрипт для запуска игры
configure_file(
    "${CMAKE_SOURCE_DIR}/wolfsp.in"
    "${CMAKE_BINARY_DIR}/wolfsp"
    @ONLY
)
