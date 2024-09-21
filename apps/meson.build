
init_sources = [
    'init.c',
    'ulib.c',
    'syscalls.S',
#    'mmc.c',
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