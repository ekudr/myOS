
lib_sources = [
    'string.c',
    'printf.c',
#    'sq_remfirst.c',
    'memset.S',
    'memmove.S',
]

libc_a = static_library(
    'libc.a',
    sources: lib_sources,
    include_directories: [
        inc_include,
    ],
    c_args: c_args,
)