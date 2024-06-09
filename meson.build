project(
    'myOS',
    ['c'],
    version : files('VERSION'),
    meson_version : '>= 1.1.0',
)

cc = meson.get_compiler('c')

c_args = [
    '-nostdlib',
    '-fno-stack-protector',
    '-static',    
]

subdir('include')
subdir('boot')