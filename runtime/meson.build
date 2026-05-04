cc = meson.get_compiler('cpp')
cuda_dep = cc.find_library(
    'cudart',
    dirs : ['/usr/lib/x86_64-linux-gnu']
)
runtime_lib = static_library(
    'runtime',
    files([
        'cuda_backend.cpp',
        'worker.cpp'
    ]),
    include_directories : [
        include_directories('.'),
        include_directories('../include'),
        include_directories('../runtime'),
        include_directories('../scheduler'),
        include_directories('../metrics'),
    
    ],
    dependencies : [cuda_dep,metrics_dep,scheduler_dep]
)