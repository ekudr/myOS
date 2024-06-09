
boot_sources = [
    'start.S',
    'boot.c',
    'printf.c',
    'uart.c'
]

executable(
    'boot.elf',
    sources : boot_sources,
    include_directories: [inc_include],
    link_args: [
        '-T boot.ld',
    ],
    link_depends: ['boot.ld'],
)