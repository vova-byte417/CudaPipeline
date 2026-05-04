cc = meson.get_compiler('cpp')

# 运行时库源文件
runtime_sources = [
    'worker.cpp',
    'cpu_backend.cpp'
]

runtime_lib = static_library(
    'runtime',
    files(runtime_sources),
    include_directories : [
        include_directories('.'),
        include_directories('../include'),
        include_directories('../runtime'),
        include_directories('../scheduler'),
        include_directories('../metrics'),
    ],
    dependencies : [metrics_dep, scheduler_dep]
)
