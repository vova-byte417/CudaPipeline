project(
  'llm-runtime-lab',
  ['cpp', 'cuda'],
  version: '0.1',
  default_options: ['cpp_std=c++17', 'warning_level=3']
)

cpp_compiler = meson.get_compiler('cpp')
cuda_compiler = meson.get_compiler('cuda')

# ========== 你的CUDA在 /usr/lib/cuda ==========
cuda_include_dir = '/usr/include'
cuda_lib_dir = '/usr/lib/cuda'

# 直接找cudart库
cudart_lib = cuda_compiler.find_library('cudart', dirs: cuda_lib_dir, required: true)

# 构造依赖
cuda_dep = declare_dependency(
  include_directories: include_directories(cuda_include_dir),
  dependencies: cudart_lib
)

# ========== 子目录 ==========
subdir('operators')
subdir('metrics')
subdir('scheduler')
subdir('runtime')
subdir('bench')
