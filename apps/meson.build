
init_sources = [
    'init.c',
    'ulib.c',
    'syscalls.S',
]


init_elf = executable(
    'init',
    sources : init_sources,
    include_directories: [inc_include],
    c_args: c_args,
    link_args: [
        '-nostdlib',
        '-static',
    ],
)

sh_sources = [
    'sh.c',
    'ulib.c',
    'syscalls.S',
]

sh_elf = executable(
    'sh',
    sources : sh_sources,
    include_directories: [inc_include],
    c_args: c_args,
    link_args: [
        '-nostdlib',
        '-static',
    ],
)