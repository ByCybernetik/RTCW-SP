project('RTCW-SP', 'c', 'cpp',
    version: '1.41',
    default_options: [
        'c_std=c99',
        'cpp_std=c++11',
        'warning_level=0',
        'buildtype=debug',
    ]
)

cc = meson.get_compiler('c')

# Architecture detection
host_cpu = host_machine.cpu_family()
if host_cpu == 'x86_64'
    arch = 'x86_64'
    arch_x86 = false
elif host_cpu == 'x86'
    arch = 'x86'
    arch_x86 = true
else
    arch = host_machine.cpu()
    arch_x86 = false
endif

message('Building for architecture: @0@'.format(arch))

# Options
vulkan_backend = get_option('vulkan_backend')
renderer_name = vulkan_backend ? 'Vulkan' : 'OpenGL'

# Dependencies
sdl2_dep = dependency('sdl2')
gl_dep = dependency('gl')
alsa_dep = dependency('alsa', required: false)
thread_dep = dependency('threads')
m_dep = cc.find_library('m', required: false)
dl_dep = cc.find_library('dl', required: false)

# Common compile arguments
common_c_args = [
    '-pipe',
    '-fsigned-char',
    '-Wall',
    '-Wno-unknown-pragmas',
    '-Wno-write-strings',
    '-Wno-deprecated-declarations',
    '-DSDL2_QGL',
]

if get_option('buildtype') == 'release'
    common_c_args += ['-O0', '-DNDEBUG']
    if arch_x86
        common_c_args += '-march=pentium'
    endif
else
    common_c_args += ['-g', '-O0']
endif

if arch_x86
    common_c_args += '-m32'
    add_global_link_arguments('-m32', language: 'c')
    add_global_link_arguments('-m32', language: 'cpp')
endif

# Include directories
inc_dirs = include_directories(
    'src/game',
    'src/qcommon',
    'src/client',
    'src/server',
    'src/unix',
    'src/botlib',
    'src/splines',
    'src/renderer',
    'src/stb',
)

# Common sources
game_common_sources = files(
    'src/game/q_shared.c',
    'src/game/q_math.c',
)

# Renderer sources
renderer_sources = files(
    'src/renderer/tr_animation.c',
    'src/renderer/tr_backend.c',
    'src/renderer/tr_bsp.c',
    'src/renderer/tr_cmds.c',
    'src/renderer/tr_cmesh.c',
    'src/renderer/tr_curve.c',
    'src/renderer/tr_flares.c',
    'src/renderer/tr_font.c',
    'src/renderer/tr_image.c',
    'src/renderer/tr_init.c',
    'src/renderer/tr_light.c',
    'src/renderer/tr_main.c',
    'src/renderer/tr_marks.c',
    'src/renderer/tr_mesh.c',
    'src/renderer/tr_model.c',
    'src/renderer/tr_noise.c',
    'src/renderer/tr_scene.c',
    'src/renderer/tr_shade.c',
    'src/renderer/tr_shade_calc.c',
    'src/renderer/tr_shader.c',
    'src/renderer/tr_shadows.c',
    'src/renderer/tr_sky.c',
    'src/renderer/tr_surface.c',
    'src/renderer/tr_world.c',
)

# Client sources
client_sources = files(
    'src/client/cl_cgame.c',
    'src/client/cl_cin.c',
    'src/client/cl_console.c',
    'src/client/cl_input.c',
    'src/client/cl_keys.c',
    'src/client/cl_main.c',
    'src/client/cl_net_chan.c',
    'src/client/cl_parse.c',
    'src/client/cl_scrn.c',
    'src/client/cl_ui.c',
    'src/client/snd_adpcm.c',
    'src/client/snd_dma.c',
    'src/client/snd_mem.c',
    'src/client/snd_mix.c',
    'src/client/snd_wavelet.c',
)

# Server sources
server_sources = files(
    'src/server/sv_bot.c',
    'src/server/sv_ccmds.c',
    'src/server/sv_client.c',
    'src/server/sv_game.c',
    'src/server/sv_init.c',
    'src/server/sv_main.c',
    'src/server/sv_net_chan.c',
    'src/server/sv_snapshot.c',
    'src/server/sv_world.c',
)

# Common (qcommon) sources
common_sources = files(
    'src/qcommon/cm_load.c',
    'src/qcommon/cm_patch.c',
    'src/qcommon/cm_polylib.c',
    'src/qcommon/cm_test.c',
    'src/qcommon/cm_trace.c',
    'src/qcommon/cmd.c',
    'src/qcommon/common.c',
    'src/qcommon/cvar.c',
    'src/qcommon/files.c',
    'src/qcommon/md4.c',
    'src/qcommon/msg.c',
    'src/qcommon/net_chan.c',
    'src/qcommon/huffman.c',
    'src/qcommon/unzip.c',
)

# VM sources
vm_sources = files(
    'src/qcommon/vm.c',
    'src/qcommon/vm_interpreted.c',
)
if arch_x86
    vm_sources += files('src/qcommon/vm_x86.c')
endif

# Unix/SDL2 platform sources
unix_sources = files(
    'src/unix/unix_main.c',
    'src/unix/unix_net.c',
    'src/unix/unix_shared.c',
    'src/unix/unix_sdl2.c',
    'src/unix/sdl2_qgl.c',
)

# Botlib sources
botlib_sources = files(
    'src/botlib/be_aas_bspq3.c',
    'src/botlib/be_aas_cluster.c',
    'src/botlib/be_aas_debug.c',
    'src/botlib/be_aas_entity.c',
    'src/botlib/be_aas_file.c',
    'src/botlib/be_aas_main.c',
    'src/botlib/be_aas_move.c',
    'src/botlib/be_aas_optimize.c',
    'src/botlib/be_aas_reach.c',
    'src/botlib/be_aas_route.c',
    'src/botlib/be_aas_routealt.c',
    'src/botlib/be_aas_routetable.c',
    'src/botlib/be_aas_sample.c',
    'src/botlib/be_ai_char.c',
    'src/botlib/be_ai_chat.c',
    'src/botlib/be_ai_gen.c',
    'src/botlib/be_ai_goal.c',
    'src/botlib/be_ai_move.c',
    'src/botlib/be_ai_weap.c',
    'src/botlib/be_ai_weight.c',
    'src/botlib/be_ea.c',
    'src/botlib/be_interface.c',
    'src/botlib/l_crc.c',
    'src/botlib/l_libvar.c',
    'src/botlib/l_log.c',
    'src/botlib/l_memory.c',
    'src/botlib/l_precomp.c',
    'src/botlib/l_script.c',
    'src/botlib/l_struct.c',
)

# Splines sources
splines_sources = files(
    'src/splines/math_angles.cpp',
    'src/splines/math_matrix.cpp',
    'src/splines/math_quaternion.cpp',
    'src/splines/math_vector.cpp',
    'src/splines/q_parse.cpp',
    'src/splines/splines.cpp',
    'src/splines/util_str.cpp',
)

# Platform dependencies
platform_deps = [sdl2_dep, gl_dep, thread_dep, m_dep, dl_dep]
if alsa_dep.found()
    platform_deps += alsa_dep
    common_c_args += '-DHAVE_ALSA'
endif

# Helper libraries
botlib_lib = static_library('botlib',
    botlib_sources,
    c_args: common_c_args + ['-DBOTLIB'],
    include_directories: inc_dirs,
)

game_common_lib = static_library('game_common',
    game_common_sources,
    c_args: common_c_args,
    include_directories: inc_dirs,
)

splines_lib = static_library('splines',
    splines_sources,
    include_directories: inc_dirs,
)

# Game library (qagame)
game_sources = files(
    'src/game/ai_cast.c',
    'src/game/ai_cast_characters.c',
    'src/game/ai_cast_debug.c',
    'src/game/ai_cast_events.c',
    'src/game/ai_cast_fight.c',
    'src/game/ai_cast_func_attack.c',
    'src/game/ai_cast_func_boss1.c',
    'src/game/ai_cast_funcs.c',
    'src/game/ai_cast_script.c',
    'src/game/ai_cast_script_actions.c',
    'src/game/ai_cast_script_ents.c',
    'src/game/ai_cast_sight.c',
    'src/game/ai_cast_think.c',
    'src/botai/ai_chat.c',
    'src/botai/ai_cmd.c',
    'src/botai/ai_dmnet.c',
    'src/botai/ai_dmq3.c',
    'src/botai/ai_main.c',
    'src/botai/ai_team.c',
    'src/game/bg_animation.c',
    'src/game/bg_misc.c',
    'src/game/bg_pmove.c',
    'src/game/bg_slidemove.c',
    'src/game/g_active.c',
    'src/game/g_alarm.c',
    'src/game/g_bot.c',
    'src/game/g_client.c',
    'src/game/g_cmds.c',
    'src/game/g_combat.c',
    'src/game/g_items.c',
    'src/game/g_main.c',
    'src/game/g_mem.c',
    'src/game/g_misc.c',
    'src/game/g_missile.c',
    'src/game/g_mover.c',
    'src/game/g_props.c',
    'src/game/g_save.c',
    'src/game/g_script.c',
    'src/game/g_script_actions.c',
    'src/game/g_session.c',
    'src/game/g_spawn.c',
    'src/game/g_svcmds.c',
    'src/game/g_syscalls.c',
    'src/game/g_target.c',
    'src/game/g_team.c',
    'src/game/g_tramcar.c',
    'src/game/g_trigger.c',
    'src/game/g_utils.c',
    'src/game/g_weapon.c',
    'src/game/q_math.c',
    'src/game/q_shared.c',
)

qagame = shared_library('qagame' + arch,
    game_sources,
    c_args: common_c_args + ['-DGAMEDLL', '-fPIC', '-O0'],
    include_directories: inc_dirs,
    dependencies: m_dep,
    name_prefix: '',
    name_suffix: 'so',
    install: true,
)

# CGame library
cgame_sources = files(
    'src/game/bg_animation.c',
    'src/game/bg_misc.c',
    'src/game/bg_pmove.c',
    'src/game/bg_slidemove.c',
    'src/cgame/cg_consolecmds.c',
    'src/cgame/cg_draw.c',
    'src/cgame/cg_drawtools.c',
    'src/cgame/cg_effects.c',
    'src/cgame/cg_ents.c',
    'src/cgame/cg_event.c',
    'src/cgame/cg_flamethrower.c',
    'src/cgame/cg_info.c',
    'src/cgame/cg_localents.c',
    'src/cgame/cg_main.c',
    'src/cgame/cg_marks.c',
    'src/cgame/cg_newDraw.c',
    'src/cgame/cg_particles.c',
    'src/cgame/cg_players.c',
    'src/cgame/cg_playerstate.c',
    'src/cgame/cg_predict.c',
    'src/cgame/cg_scoreboard.c',
    'src/cgame/cg_servercmds.c',
    'src/cgame/cg_snapshot.c',
    'src/cgame/cg_sound.c',
    'src/cgame/cg_syscalls.c',
    'src/cgame/cg_trails.c',
    'src/cgame/cg_view.c',
    'src/cgame/cg_weapons.c',
    'src/game/q_math.c',
    'src/game/q_shared.c',
    'src/ui/ui_shared.c',
)

cgame = shared_library('cgame' + arch,
    cgame_sources,
    c_args: common_c_args + ['-DCGAMEDLL', '-fPIC', '-O0'],
    include_directories: inc_dirs,
    dependencies: m_dep,
    name_prefix: '',
    name_suffix: 'so',
    install: true,
)

# UI library
ui_sources = files(
    'src/game/bg_misc.c',
    'src/game/q_math.c',
    'src/game/q_shared.c',
    'src/ui/ui_atoms.c',
    'src/ui/ui_gameinfo.c',
    'src/ui/ui_main.c',
    'src/ui/ui_players.c',
    'src/ui/ui_shared.c',
    'src/ui/ui_syscalls.c',
    'src/ui/ui_util.c',
)

ui = shared_library('ui' + arch,
    ui_sources,
    c_args: common_c_args + ['-fPIC', '-O0'],
    include_directories: inc_dirs,
    dependencies: m_dep,
    name_prefix: '',
    name_suffix: 'so',
    install: true,
)

# Main executable
wolfsp_deps = platform_deps
wolfsp_link_with = [botlib_lib, game_common_lib, splines_lib]

if vulkan_backend
    subdir('src/vulkan')
    common_c_args += '-DVULKAN_BACKEND'
    wolfsp_deps += vulkan_dep
    wolfsp_link_with += vk_renderer
endif

wolfsp_sources = [
    renderer_sources,
    client_sources,
    server_sources,
    common_sources,
    vm_sources,
    unix_sources,
]

wolfsp = executable('wolfsp_' + arch,
    wolfsp_sources,
    c_args: common_c_args,
    include_directories: inc_dirs,
    link_with: wolfsp_link_with,
    dependencies: wolfsp_deps,
    link_args: '-rdynamic',
    install: true,
)

# Standalone BSP viewer
bsp_viewer_gl = executable('bsp_viewer_gl',
    'src/tools/bsp_viewer_gl.c',
    c_args: common_c_args,
    include_directories: inc_dirs,
    dependencies: platform_deps,
    install: true,
)

# Copy game .so files into build/main/
main_dir = 'main'

custom_target('copy-qagame',
    input: qagame,
    output: 'qagame_copy.stamp',
    command: ['sh', '-c', 'mkdir -p ' + main_dir + ' && cp @INPUT@ ' + main_dir + '/qagame' + arch + '.so && touch @OUTPUT@'],
    build_by_default: true,
)

custom_target('copy-cgame',
    input: cgame,
    output: 'cgame_copy.stamp',
    command: ['sh', '-c', 'mkdir -p ' + main_dir + ' && cp @INPUT@ ' + main_dir + '/cgame' + arch + '.so && touch @OUTPUT@'],
    build_by_default: true,
)

custom_target('copy-ui',
    input: ui,
    output: 'ui_copy.stamp',
    command: ['sh', '-c', 'mkdir -p ' + main_dir + ' && cp @INPUT@ ' + main_dir + '/ui' + arch + '.so && touch @OUTPUT@'],
    build_by_default: true,
)

# Vulkan shaders
if vulkan_backend
    glslc = find_program('glslc')

    shader_specs = [
        ['src/vulkan/shaders/world.vert', 'world_vert.spv', 'vertex'],
        ['src/vulkan/shaders/world.frag', 'world_frag.spv', 'fragment'],
        ['src/vulkan/shaders/2d_vert.glsl', '2d_vert.spv', 'vertex'],
        ['src/vulkan/shaders/2d_frag.glsl', '2d_frag.spv', 'fragment'],
    ]

    shader_targets = []
    foreach spec : shader_specs
        src = spec[0]
        out = spec[1]
        stage = spec[2]
        t = custom_target(out + '_stamp',
            input: src,
            output: out + '_stamp',
            command: ['sh', '-c', 'mkdir -p ' + main_dir + ' && ' + glslc.full_path() + ' -fshader-stage=' + stage + ' -o ' + main_dir + '/' + out + ' @INPUT@ && touch @OUTPUT@'],
            build_by_default: true,
        )
        shader_targets += t
    endforeach
endif

# Summary
summary({
    'Version': meson.project_version(),
    'Architecture': arch,
    'Build type': get_option('buildtype'),
    'Install prefix': get_option('prefix'),
    'SDL2': sdl2_dep.found(),
    'Renderer': renderer_name,
}, section: 'RTCW-SP Build Configuration')
