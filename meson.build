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
    '-mcmodel=medany',   
#    '-mcpu=sifive-u74',
    '-mtune=sifive-7-series',
    '-mno-relax',
    '-ffreestanding',
    '-fno-omit-frame-pointer',
    '-O',
]

board = get_option('board')

if board.contains('vf2')
    c_args += '-D_JH7110_'
endif

if board.contains('qemu')
    c_args += '-D_SIFIVE_U_'
endif

subdir('include')
subdir('lib')
subdir('kernel')
subdir('drivers')
subdir('boot')


image = custom_target('image',
    input: boot_elf,
    output: 'OSImage.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    depends: boot_elf,
    build_by_default: true
)