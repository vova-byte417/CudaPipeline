metrics_lib = static_library(
    'metrics',
    files([
        'metrics.cpp'
    ]),
    include_directories : include_directories('../include')
)

metrics_dep = declare_dependency(
    link_with : metrics_lib,
    include_directories : include_directories('../include')
)