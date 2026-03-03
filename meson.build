project('chip8-emu'
    , 'c'
    , version: '1.0'
    , default_options: {
          'c_std': 'c23'
        , 'buildtype': 'debug'
        , 'optimization': 'g'
        , 'prefer_static': true
        , 'warning_level': '3'
        , 'werror': true
    }
)

inc = include_directories('include')
src = files(
      'src/main.c'
    , 'src/rom.c'
    , 'src/chip.c'
    , 'src/argv.c'
    , 'src/vm.c'
    , 'src/rand.c'
    , 'src/util.c'
    , 'src/allocator.c'
)

sdl = dependency('sdl3')
executable('chip8'
    , src
    , include_directories: inc
    , dependencies: [sdl]
)
