
mem_sources = [
    'pgman.c',
    'mmu.c',
    'mem.c',
    'mem_kstack.c',
]


mem_a = static_library(
    'mem.a',
    sources: mem_sources,
    include_directories: [
        inc_include,
    ],
    c_args: c_args,
)