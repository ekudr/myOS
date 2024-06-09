
boot_sources = [
    'start.S',
    'boot.c',
    'printf.c',
    'uart.c'
]

linker_scripts = ['boot.ld']
linker_script_flags = []

foreach entry : meson.get_cross_property('linker_scripts', [''])
    if entry != ''
        linker_script_flags += '-T' + entry
    endif
endforeach

foreach entry : meson.get_cross_property('link_depends', [''])
    if entry != ''
        linker_script_deps += meson.source_root() / entry
    endif
endforeach

executable(
    'boot.elf',
    sources : boot_sources,
    include_directories: [inc_include],
    link_args: [
        linker_script_flags,
    ],
    link_depends: linker_script_deps,
)