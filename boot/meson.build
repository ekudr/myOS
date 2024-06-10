
boot_sources = [
    'start.S',
    'boot.c',
    'printf.c',
    'uart.c',
    'mm_init.c',
]

linker_scripts = [
    'boot.ld',
]

linker_script_flags = []
linker_script_deps = []

foreach entry : linker_scripts
    if entry != ''
        linker_script_flags += '-T' + meson.current_source_dir() / entry
    endif
endforeach

foreach entry : linker_scripts
    if entry != ''
        linker_script_deps += meson.current_source_dir() / entry
    endif
endforeach

boot_elf = executable(
    'boot.elf',
    sources : boot_sources,
    include_directories: [inc_include],
    c_args: c_args,
    link_args: [
        '-nostdlib',
        '-static',
        linker_script_flags,
    ],
    link_depends: linker_script_deps,
)