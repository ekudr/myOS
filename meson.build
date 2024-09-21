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
#    '-mtune=sifive-7-series',
#    '-mno-relax',
    '-ffreestanding',
#    '-fno-omit-frame-pointer',
    '-O3',
]

board = get_option('board')


subdir('include')

if board.contains('vf2')
    inc_include += include_directories('./board/vf2/include')
    c_args += '-mtune=sifive-7-series'
endif
if board.contains('qemu')
    inc_include += include_directories('./board/qemu/include')
endif

if board.contains('bpi-f3')
    inc_include += include_directories('./board/bpi-f3/include')
#    c_args += '-march=rv64imafdc_zba_zbb_zbc_zbs_zicsr_zifencei_zicbom'
endif

subdir('lib')
subdir('kernel')
subdir('drivers')
subdir('board')
subdir('apps')


image = custom_target('image',
    input: myos_elf,
    output: 'OSImage.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
    depends: myos_elf,
    build_by_default: true
)

summary(
  {
    'Built type': get_option('buildtype'),
  },
  section: 'Build settings'
)

summary(
  {
    'Board': get_option('board'),
  },
  section: 'Platform'
)
