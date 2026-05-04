executable(
    'runtime_bench',
    'main.cpp',

    include_directories : [
        include_directories('../include'),
        include_directories('/usr/include'),
        include_directories('../runtime'),
        include_directories('../scheduler'),
        include_directories('../metrics'),
    ],

    link_with : runtime_lib,
    link_args : ['-lcudart']
)

# 优先级调度器 benchmark
executable(
    'priority_bench',
    'priority_bench.cpp',

    include_directories : [
        include_directories('../include'),
        include_directories('/usr/include'),
        include_directories('../runtime'),
        include_directories('../scheduler'),
        include_directories('../metrics'),
    ],

    link_with : runtime_lib,
    link_args : ['-lcudart']
)

# EDF 调度器 benchmark
executable(
    'edf_bench',
    'edf_bench.cpp',

    include_directories : [
        include_directories('../include'),
        include_directories('/usr/include'),
        include_directories('../runtime'),
        include_directories('../scheduler'),
        include_directories('../metrics'),
    ],

    link_with : runtime_lib,
    link_args : ['-lcudart']
)