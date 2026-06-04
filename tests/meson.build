
utils_tests = executable('utils_tests', 'url_utils_tests.c',
	dependencies : [utils_lib_dep]
)

test('utils_tests', utils_tests)


gtest_dep = dependency('gtest', main : true)
gtest_main_dep = dependency('gtest_main')
environment_tests = executable(
    'environment_tests',
    [
        'environment_tests.cpp',
    ],
    include_directories: ['../src'],
    dependencies: [gtest_dep, gtest_main_dep, utils_lib_dep],
    build_by_default: false
)
require_version_tests = executable(
    'require_version_tests',
    [
        'require_version_tests.cpp',
    ],
    include_directories: ['../src'],
    dependencies: [gtest_dep, gtest_main_dep, utils_lib_dep],
    build_by_default: false
)

test('environment_tests', environment_tests)
test('require_version_tests', require_version_tests)

set_array_tests = executable(
    'set_array_tests',
    sources: ['param_slash_parse_array_tests.c', '../src/param_slash.c', '../src/known_hosts.c',],
    dependencies: [csp_dep, slash_dep, param_dep, utils_lib_dep],
)
test('set_array_tests', set_array_tests)

# Regression for the hk_param_sniffer() data_len underflow segfault (short sport-13
# DIPP RPC replies). Compiles the real hk_param_sniffer.c; crash is caught via fork().
hk_sniffer_tests = executable(
    'hk_sniffer_tests',
    sources: ['hk_sniffer_host.c', '../src/hk_param_sniffer.c'],
    include_directories: ['../src'],
    dependencies: [csp_dep, slash_dep, param_dep, utils_lib_dep],
)
test('hk_sniffer_tests', hk_sniffer_tests)
