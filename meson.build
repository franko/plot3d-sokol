project('plot3d', 'c', default_options : ['c_std=c99'])

if host_machine.system() == 'darwin'
    add_languages('objc')
    # We may use Metal on macOS but we use SDL2 to manage the window and SDL
    # works naturally with OpenGL.
    plot3d_slang = 'glsl330'
    plot3d_backend = 'GLCORE33'
    sokol_impl_src = 'sokol_gfx.m'
else
    plot3d_slang = 'glsl330'
    plot3d_backend = 'GLCORE33'
    sokol_impl_src = 'sokol_gfx.c'
endif

plot3d_deps = []
plot3d_deps += dependency('handmademath')
plot3d_deps += dependency('sokol')
plot3d_deps += dependency('sdl2', method: 'config-tool')
plot3d_deps += dependency('gl')

sokol_shdc = find_program('sokol-shdc')

plot3d_c_args = ['-DSOKOL_' + plot3d_backend]

plot3d_shader = custom_target('plot3d-glsl-h',
    input : 'plot3d.glsl',
    output : 'plot3d.glsl.h',
    command : [sokol_shdc, '--input', '@INPUT@', '--output', '@OUTPUT0@', '--slang', plot3d_slang]
)

plot3d_link_args = []
if host_machine.system() == 'darwin'
    plot3d_link_args += ['-framework', 'OpenGL']
endif

executable('plot3d-test', ['plot3d-test.c', plot3d_shader, sokol_impl_src],
    c_args : plot3d_c_args,
    objc_args : plot3d_c_args,
    link_args : plot3d_link_args,
    dependencies : plot3d_deps,
    win_subsystem : 'windows'
)

