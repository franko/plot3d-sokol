project('plot3d', 'c', default_options : ['c_std=c99'])

plot3d_slang = 'glsl330'

plot3d_deps = []
plot3d_deps += dependency('handmademath')
plot3d_deps += dependency('sokol')
plot3d_deps += dependency('sdl2', method: 'config-tool')
plot3d_deps += dependency('gl')

sokol_shdc = find_program('sokol-shdc')

plot3d_c_args = []

plot3d_shader = custom_target('plot3d-glsl-h',
    input : 'plot3d.glsl',
    output : 'plot3d.glsl.h',
    command : [sokol_shdc, '--input', '@INPUT@', '--output', '@OUTPUT0@', '--slang', plot3d_slang]
)

executable('plot3d-test', ['plot3d-test.c', plot3d_shader],
    c_args : plot3d_c_args,
    dependencies : plot3d_deps,
    win_subsystem : 'windows'
)

