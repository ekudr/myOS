drivers_sources = [
    'spi.c',
    'uart.c',
    'disk.c',
    'riscv_timer.c',
    'mmc_dev.c',
    'console.c',    
#     'uart.c',
#    'kernelvec.S',
#    'trampoline.S',
#    'plic.c',
#    'syscall.c',
]

if board.contains('vf2')
    drivers_sources += ['mmc.c',
                        'dw_mmc.c',
        ]
endif

if board.contains('bpi-f3')
    drivers_sources += ['sdhci.c',
                        'spacemit_sdhci.c',
                        'mmc.c',
                        'pinctrl.c'
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