project(
    'xrfc',
    'cpp',
    default_options: [
        'cpp_std=c++17',
        'warning_level=3',
        'werror=true',
    ],
    version: '0.0.1',
)

catch2_proj = subproject('catch2')
catch2_dep = catch2_proj.get_variable('catch2_dep')

cli11_proj = subproject('cli11')
cli11_dep = cli11_proj.get_variable('CLI11_dep')

llvm_dep = dependency('llvm', version: ['>=10'])

xrf_deps = [
    cli11_dep,
    llvm_dep,
]

xrfc = executable(
    'xrfc',
    'src/main.cpp',
    dependencies: xrf_deps,
    install: true,
)
