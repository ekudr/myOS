project(
    'myOS',
    ['c'],
    version : files('VERSION'),
    meson_version : '>= 1.1.0',
)

cc = meson.get_compiler('c')
objcopy = find_program('objcopy')

c_args = [
    '-nostdlib',
    '-fno-stack-protector',
    '-static',    
]


subdir('include')
subdir('boot')


image = custom_target('image',
    input: boot_elf,
    output: 'OSImage.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    depends: boot_elf,
    build_by_default: true
)