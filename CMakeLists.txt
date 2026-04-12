# CMakeLists.txt для Return to Castle Wolfenstein
# Адаптировано для современных Linux систем
# - Сборщик: CMake
# - Окно/ввод/OpenGL: GLFW
# - Звук: PulseAudio
# - Архитектура: x64
# - Библиотеки: стандартные .so (без QVM)

cmake_minimum_required(VERSION 3.16)
project(rtcmw VERSION 1.41 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Опции сборки
option(BUILD_CLIENT "Собрать клиент" ON)
option(BUILD_SERVER "Собрать сервер" OFF)
option(BUILD_GAME "Собрать игровой модуль" ON)
option(BUILD_CGAME "Собрать клиентский игровой модуль" ON)
option(BUILD_UI "Собрать UI модуль" ON)
option(USE_PULSEAUDIO "Использовать PulseAudio для звука" ON)
option(USE_GLFW "Использовать GLFW для окна и ввода" ON)

# Поиск зависимостей
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)

if(USE_GLFW)
    find_package(glfw3 3.3 REQUIRED)
endif()

if(USE_PULSEAUDIO)
    pkg_check_modules(PULSEAUDIO REQUIRED libpulse-simple libpulse)
else()
    pkg_check_modules(ALSA REQUIRED alsa)
endif()

pkg_check_modules(FREETYPE2 REQUIRED freetype2)
pkg_check_modules(JPEG REQUIRED libjpeg)

# Пути к исходникам
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(IDLIB_DIR ${SRC_DIR}/idLib)
set(QCOMMON_DIR ${SRC_DIR}/qcommon)
set(CLIENT_DIR ${SRC_DIR}/client)
set(RENDERER_DIR ${SRC_DIR}/renderer)
set(GAME_DIR ${SRC_DIR}/game)
set(CGAME_DIR ${SRC_DIR}/cgame)
set(UI_DIR ${SRC_DIR}/ui)
set(BOTLIB_DIR ${SRC_DIR}/botlib)
set(SPLINES_DIR ${SRC_DIR}/splines)
set(FT2_DIR ${SRC_DIR}/ft2)
set(JPEG_DIR ${SRC_DIR}/jpeg-6)

# Общие определения
add_definitions(
    -DQ3_VERSION=Wolf141
    -DLINUX
    -D__linux__
    -DARCH_X86_64
    -DGLFW_INCLUDE
    -DPULSE_AUDIO
    -Wall
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-misleading-indentation
    -Wno-format-truncation
)

# Путь поиска заголовков
include_directories(
    ${SRC_DIR}
    ${IDLIB_DIR}
    ${QCOMMON_DIR}
    ${CLIENT_DIR}
    ${RENDERER_DIR}
    ${GAME_DIR}
    ${CGAME_DIR}
    ${UI_DIR}
    ${BOTLIB_DIR}
    ${SPLINES_DIR}
    ${FT2_DIR}
    ${JPEG_DIR}
    ${FREETYPE2_INCLUDE_DIRS}
    ${JPEG_INCLUDE_DIRS}
    ${PULSEAUDIO_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

# ============================================================
# БИБЛИОТЕКА idLib
# ============================================================
set(IDLIB_SOURCES
    ${IDLIB_DIR}/idQ3Asnd.cpp
    ${IDLIB_DIR}/idSound.cpp
    ${IDLIB_DIR}/idSpeaker.cpp
)

add_library(idlib STATIC ${IDLIB_SOURCES})

# ============================================================
# БИБЛИОТЕКА qcommon (общий код)
# ============================================================
set(QCOMMON_SOURCES
    ${QCOMMON_DIR}/common.c
    ${QCOMMON_DIR}/msg.c
    ${QCOMMON_DIR}/md4.c
    ${QCOMMON_DIR}/vm.c
    ${QCOMMON_DIR}/files.c
    ${QCOMMON_DIR}/huffman.c
    ${QCOMMON_DIR}/cmd.c
    ${QCOMMON_DIR}/cvar.c
    ${QCOMMON_DIR}/net_chan.c
    ${QCOMMON_DIR}/cm_load.c
    ${QCOMMON_DIR}/cm_polylib.c
    ${QCOMMON_DIR}/cm_patch.c
    ${QCOMMON_DIR}/cm_test.c
)

add_library(qcommon STATIC ${QCOMMON_SOURCES})

# ============================================================
# БИБЛИОТЕКА splines
# ============================================================
set(SPLINES_SOURCES
    ${SPLINES_DIR}/math_angles.cpp
    ${SPLINES_DIR}/math_matrix.cpp
    ${SPLINES_DIR}/math_quaternion.cpp
    ${SPLINES_DIR}/math_vector.cpp
    ${SPLINES_DIR}/util_str.cpp
    ${SPLINES_DIR}/q_parse.cpp
    ${SPLINES_DIR}/q_shared.cpp
    ${SPLINES_DIR}/splines.cpp
)

add_library(splines STATIC ${SPLINES_SOURCES})

# ============================================================
# БИБЛИОТЕКА botlib
# ============================================================
file(GLOB BOTLIB_SOURCES ${BOTLIB_DIR}/*.c)
add_library(botlib STATIC ${BOTLIB_SOURCES})

# ============================================================
# БИБЛИОТЕКА renderer
# ============================================================
file(GLOB RENDERER_SOURCES ${RENDERER_DIR}/*.c)
# Исключаем старые файлы инициализации OpenGL
list(REMOVE_ITEM RENDERER_SOURCES 
    ${RENDERER_DIR}/tr_init_old.c
)

add_library(renderer STATIC ${RENDERER_SOURCES})
target_link_libraries(renderer ${OPENGL_gl_LIBRARY})

# ============================================================
# КЛИЕНТ (основной исполняемый файл)
# ============================================================
if(BUILD_CLIENT)
    set(CLIENT_SOURCES
        ${CLIENT_DIR}/cl_main.c
        ${CLIENT_DIR}/cl_cgame.c
        ${CLIENT_DIR}/cl_cin.c
        ${CLIENT_DIR}/cl_console.c
        ${CLIENT_DIR}/cl_input.c
        ${CLIENT_DIR}/cl_keys.c
        ${CLIENT_DIR}/cl_net_chan.c
        ${CLIENT_DIR}/cl_parse.c
        ${CLIENT_DIR}/cl_scrn.c
        ${CLIENT_DIR}/cl_ui.c
        ${CLIENT_DIR}/snd_dma.c
        ${CLIENT_DIR}/snd_mem.c
        ${CLIENT_DIR}/snd_mix.c
        ${CLIENT_DIR}/snd_adpcm.c
        ${CLIENT_DIR}/snd_wavelet.c
    )

    # Файлы платформы Linux с GLFW
    set(PLATFORM_SOURCES
        ${SRC_DIR}/unix/linux_glimp_glfw.c
        ${SRC_DIR}/unix/unix_main_glfw.c
        ${SRC_DIR}/unix/linux_snd_pulse.c
        ${SRC_DIR}/unix/unix_shared.c
        ${SRC_DIR}/unix/unix_net.c
    )

    add_executable(wolfsp
        ${CLIENT_SOURCES}
        ${PLATFORM_SOURCES}
    )

    target_link_libraries(wolfsp
        idlib
        qcommon
        splines
        renderer
        botlib
        glfw
        ${PULSEAUDIO_LIBRARIES}
        ${FREETYPE2_LIBRARIES}
        ${JPEG_LIBRARIES}
        ${OPENGL_gl_LIBRARY}
        pthread
        m
        dl
    )

    target_link_directories(wolfsp PRIVATE
        ${PULSEAUDIO_LIBRARY_DIRS}
        ${FREETYPE2_LIBRARY_DIRS}
        ${JPEG_LIBRARY_DIRS}
    )
endif()

# ============================================================
# ИГРОВОЙ МОДУЛЬ (game.so)
# ============================================================
if(BUILD_GAME)
    file(GLOB GAME_SOURCES ${GAME_DIR}/*.c)
    # Исключаем тестовые файлы
    list(REMOVE_ITEM GAME_SOURCES ${GAME_DIR}/*test*.c)

    add_library(game SHARED ${GAME_SOURCES})
    target_link_libraries(game idlib splines botlib)
    set_target_properties(game PROPERTIES PREFIX "")
endif()

# ============================================================
# КЛИЕНТСКИЙ ИГРОВОЙ МОДУЛЬ (cgame.so)
# ============================================================
if(BUILD_CGAME)
    file(GLOB CGAME_SOURCES ${CGAME_DIR}/*.c)

    add_library(cgame SHARED ${CGAME_SOURCES})
    target_link_libraries(cgame idlib splines)
    set_target_properties(cgame PROPERTIES PREFIX "")
endif()

# ============================================================
# UI МОДУЛЬ (ui.so)
# ============================================================
if(BUILD_UI)
    file(GLOB UI_SOURCES ${UI_DIR}/*.c)

    add_library(ui SHARED ${UI_SOURCES})
    target_link_libraries(ui idlib splines)
    set_target_properties(ui PROPERTIES PREFIX "")
endif()

# ============================================================
# СЕРВЕР (опционально)
# ============================================================
if(BUILD_SERVER)
    set(SERVER_SOURCES
        ${SERVER_DIR}/sv_main.c
        ${SERVER_DIR}/sv_init.c
        ${SERVER_DIR}/sv_ccmds.c
        ${SERVER_DIR}/sv_game.c
        ${SERVER_DIR}/sv_user.c
        ${SERVER_DIR}/sv_world.c
        ${SERVER_DIR}/sv_bot.c
        ${SERVER_DIR}/sv_snapshot.c
    )

    add_executable(wolfsp_dedicated
        ${SERVER_SOURCES}
        ${SRC_DIR}/unix/unix_main_dedicated.c
        ${SRC_DIR}/unix/unix_shared.c
        ${SRC_DIR}/unix/unix_net.c
    )

    target_link_libraries(wolfsp_dedicated
        idlib
        qcommon
        botlib
        pthread
        m
        dl
    )
endif()

# ============================================================
# УСТАНОВКА
# ============================================================
install(TARGETS wolfsp DESTINATION bin)

if(BUILD_GAME)
    install(TARGETS game DESTINATION lib/rtcw)
endif()

if(BUILD_CGAME)
    install(TARGETS cgame DESTINATION lib/rtcw)
endif()

if(BUILD_UI)
    install(TARGETS ui DESTINATION lib/rtcw)
endif()

# Копирование скриптов запуска
configure_file(
    ${CMAKE_SOURCE_DIR}/scripts/rtcw.sh.in
    ${CMAKE_BINARY_DIR}/rtcw.sh
    @ONLY
)
