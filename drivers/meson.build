drivers_sources = [
    'spi.c',
    'sd.c',
    'mmc.c',
    'riscv_timer.c',
#    'mmu.c',
#    'trap.c',
#    'kernelvec.S',
#    'trampoline.S',
#    'plic.c',
#    'console.c',
#    'syscall.c',
]

drivers_a = static_library(
    'drivers.a',
    sources: drivers_sources,
    include_directories: [
        inc_include,
    ],
    c_args: c_args,
)