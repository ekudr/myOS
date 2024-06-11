
kernel_sources = [
    'sched.c',
    'spinlock.c',
    'switch',
]

libc_a = static_library(
    'kernel.a',
    sources: kernel_sources,
    include_directories: [
        inc_include,
    ],
    c_args: c_args,
)