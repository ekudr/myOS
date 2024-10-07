
kernel_sources = [
    'sched.c',
    'spinlock.c',
    'switch.S',
    'trap.c',
    'kernelvec.S',
    'trampoline.S',
    'plic.c',
    'init.c',
    'syscall.c',
    'sbi.c',
    'exec.c',
    'inittask.c',
]

subdir('mem')

kernel_a = static_library(
    'kernel.a',
    sources: kernel_sources,
    include_directories: [
        inc_include,
    ],
    c_args: c_args,
    link_with: [
        mem_a,
    ],
    link_depends: [
        mem_a,
    ],
)