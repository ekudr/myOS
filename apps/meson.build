
init_sources = [
    'init.c',
    'ulib.c',
#    'sd.c',
#    'mmc.c',
]


init_elf = executable(
    'init',
    sources : init_sources,
    c_args: c_args,
    link_args: [
        '-nostdlib',
        '-static',
    ],
)