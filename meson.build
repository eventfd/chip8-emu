project('chip8-emu', 'c', default_options: ['c_std=c17'], version: '1.0')

inc = include_directories('include')
src = files('src/main.c')
sdl = dependency('sdl3')

executable('chip8'
    , src
    , include_directories: inc
    , dependencies: [sdl])
