project(
  'cudapipeline',
  ['cpp'],
  version: '2.0.0',
  default_options: ['cpp_std=c++17', 'warning_level=1']
)

cpp = meson.get_compiler('cpp')

# 线程依赖
thread_dep = dependency('threads')

# 头文件目录
inc_dirs = include_directories('include')
bench_dirs = include_directories('include', '.')

# 编译静态库
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

# Harness 框架库
harness_lib = static_library(
  'harness',
  'harness/resource_quota.cpp',
  include_directories: inc_dirs
)

# CLI 工具
executable(
  'cudapipeline',
  'src/cudapipeline.cpp',
  include_directories: bench_dirs,
  link_with: [runtime_lib, scheduler_lib, metrics_lib]
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

# Harness v2.0 - Skill 隔离框架
executable(
  'harness_bench',
  'bench/harness_bench.cpp',
  include_directories: bench_dirs,
  link_with: harness_lib,
  dependencies: thread_dep
)
