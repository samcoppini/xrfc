project(
    'xrfc',
    'cpp',
    default_options: [
        'cpp_std=c++17',
        'warning_level=3',
    ],
    version: '0.0.1',
)

catch2_proj = subproject('catch2')
catch2_dep = catch2_proj.get_variable('catch2_dep')

cli11_proj = subproject('cli11')
cli11_dep = cli11_proj.get_variable('CLI11_dep')

llvm_dep = dependency('llvm', version: ['>=10'])

xrf_inc = include_directories('inc')

xrf_lib = static_library(
    'xrf-lib',
    'src/codegen.cpp',
    'src/file-reader.cpp',
    'src/optimization.cpp',
    'src/parser.cpp',
    dependencies: llvm_dep,
    include_directories: xrf_inc,
)

xrf_dep = declare_dependency(
    dependencies: llvm_dep,
    include_directories: xrf_inc,
    link_with: xrf_lib,
)

xrfc = executable(
    'xrfc',
    'src/main.cpp',
    cpp_args: [
        '-DXRFC_VERSION="@0@"'.format(meson.project_version()),
    ],
    dependencies: [
        cli11_dep,
        xrf_dep,
    ],
    install: true,
)

xrf_test_exe = executable(
    'xrf_test',
    'test/parser-test.cpp',
    'test/test-main.cpp',
    dependencies: [
        catch2_dep,
        xrf_dep,
    ],
)

test(
    'xrfc_tests',
    xrf_test_exe,
)
