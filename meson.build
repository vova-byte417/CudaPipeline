project(
  'cudapipeline',
  ['cpp'],
  version: '1.0.0',
  default_options: ['cpp_std=c++17', 'warning_level=1']
)

cpp = meson.get_compiler('cpp')

# 线程依赖
thread_dep = dependency('threads')

# 头文件目录
inc_dirs = include_directories('include')

# 编译调度器和运行时为静态库
scheduler_lib = static_library(
  'scheduler',
  'scheduler/fcfs_scheduler.cpp',
  include_directories: inc_dirs
)

runtime_lib = static_library(
  'runtime',
  'runtime/cpu_backend.cpp',
  'runtime/worker.cpp',
  include_directories: inc_dirs
)

metrics_lib = static_library(
  'metrics',
  'metrics/metrics.cpp',
  include_directories: inc_dirs
)

# Benchmark 程序
bench_dirs = [
  include_directories('include'),
  include_directories('runtime'),
  include_directories('scheduler'),
  include_directories('metrics')
]

# 主 CLI 工具
executable(
  'cudapipeline',
  'src/cudapipeline.cpp',
  include_directories: bench_dirs,
  link_with: [runtime_lib, scheduler_lib, metrics_lib],
  install: true
)

# Benchmark 程序
executable(
  'runtime_bench',
  'bench/main.cpp',
  include_directories: bench_dirs,
  link_with: [runtime_lib, scheduler_lib, metrics_lib]
)

executable(
  'priority_bench',
  'bench/priority_bench.cpp',
  include_directories: bench_dirs,
  link_with: [runtime_lib, scheduler_lib, metrics_lib]
)

executable(
  'edf_bench',
  'bench/edf_bench.cpp',
  include_directories: bench_dirs,
  link_with: [runtime_lib, scheduler_lib, metrics_lib]
)

executable(
  'rl_bench',
  'bench/rl_bench.cpp',
  include_directories: bench_dirs,
  link_with: [runtime_lib, scheduler_lib, metrics_lib]
)
