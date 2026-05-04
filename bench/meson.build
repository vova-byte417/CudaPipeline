executable(
    'runtime_bench',
    'main.cpp',

    include_directories : [
        include_directories('../include'),
        include_directories('/usr/include'),
        include_directories('../runtime'),
        include_directories('../scheduler'),   # ← 加上这一行
        include_directories('../metrics'),
    ],

    link_with : runtime_lib,
    link_args : ['-lcudart']
)