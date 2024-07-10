drivers_sources = [
    'spi.c',
    'uart.c',
#    'mmc.c',
    'riscv_timer.c',
#    'dw_mmc.c',
#     'uart.c',
#    'kernelvec.S',
#    'trampoline.S',
#    'plic.c',
#    'console.c',
#    'syscall.c',
]

if board.contains('vf2')
    drivers_sources += ['mmc.c',
                        'dw_mmc.c',
        ]
endif

if board.contains('qemu')
   drivers_sources += [
                        'sd.c',
                        'uart_sifive.c',
    ]
endif

drivers_a = static_library(
    'drivers.a',
    sources: drivers_sources,
    include_directories: [
        inc_include,
    ],
    c_args: c_args,
)