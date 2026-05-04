project(
  'llm-runtime-lab',
  ['cpp', 'cuda'],
  version: '0.1'
)

cuda = meson.get_compiler('cuda')
cpp = meson.get_compiler('cpp')

subdir('operators')
subdir('metrics')
subdir('scheduler')
subdir('runtime')
subdir('bench')
