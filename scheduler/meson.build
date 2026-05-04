# scheduler/meson.build

scheduler_lib = static_library(
    'scheduler',
    files([
        'fcfs_scheduler.cpp'
    ]),
    include_directories : include_directories('../include')
)

scheduler_dep = declare_dependency(
    link_with : scheduler_lib,
    include_directories : include_directories('../include')
)