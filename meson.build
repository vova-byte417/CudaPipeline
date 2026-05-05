project(
  'cudapipeline',
  ['cpp'],
  version: '2.0.0',
  default_options: ['cpp_std=c++17', 'warning_level=1', 'cpp_threads=true']
)

cpp = meson.get_compiler('cpp')

# 线程依赖
thread_dep = dependency('threads')

# 头文件目录
inc_dirs = include_directories('include', 'tests')

# ========== 核心库 ==========

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

# Harness v2.0 企业级框架
harness_lib = static_library(
  'harness',
  'harness/resource_quota.cpp',
  include_directories: inc_dirs
)

# ========== 基准测试程序 ==========

bench_dirs = include_directories('include', '.', 'tests')

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

# Harness v2.0 测试
executable(
  'harness_bench',
  'bench/harness_bench.cpp',
  include_directories: bench_dirs,
  link_with: harness_lib,
  dependencies: thread_dep
)

# ========== CLI 工具 ==========

executable(
  'cudapipeline',
  'src/cudapipeline.cpp',
  include_directories: bench_dirs,
  link_with: [runtime_lib, scheduler_lib, metrics_lib]
)

# ========== 单元测试 & 集成测试 ==========

# 单元测试源文件
unit_test_sources = [
  'tests/main.cpp',
  'tests/unit/test_resource_quota.cpp',
  'tests/unit/test_schedulers.cpp',
]

# 集成测试源文件
integration_test_sources = [
  'tests/main.cpp',
  'tests/unit/test_resource_quota.cpp',
  'tests/unit/test_schedulers.cpp',
  'tests/integration/test_end_to_end.cpp',
]

# 完整测试套件
executable(
  'test_runner',
  integration_test_sources,
  include_directories: bench_dirs,
  link_with: [scheduler_lib, harness_lib],
  dependencies: thread_dep
)
