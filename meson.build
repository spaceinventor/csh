project('csh', ['c', 'cpp'], subproject_dir: 'lib', default_options: [
	'buildtype=debug', 
	'optimization=2',
	'warning_level=0',
	'c_std=gnu11', 
	'b_lto=false',
	'default_library=static',
	'slash:builtins=true',
	'csp:packet_padding_bytes=42', 
	'csp:buffer_count=1000',
	'csp:buffer_size=2048',
	'csp:conn_max=20',
	'csp:conn_rxqueue_len=1000',
	'csp:qfifo_len=1000',
	'csp:rdp_max_window=1000',
	'csp:port_max_bind=16',
	'csp:use_rtable=true',
	'param:have_fopen=true', 
	'param:collector=false', 
	'param:list_dynamic=true'])

# Adding -Wall and -Wextra generate harmless (or senseless if you ask me) Meson warnings:
# 'Consider using the built-in warning_level option instead of using "-Wall"'
# 'Consider using the built-in warning_level option instead of using "-Wextra"'
# which would be fine *IF* doing so didn't automatically propagate these options to this project's dependencies.
# As we would like (at least for now) to only care about the warnings in this project's source code,
# we don't want to propagate our warning settings to them, these dependencies can use whatever warning settings
# they want.
# As Meson doesn't propagate the arguments in add_project_arguments() to the dependencies, we can
# accept the Meson warnings given the flexibilty we gain
add_project_arguments(['-I../include', '-Wall', '-Wextra', '-Wno-unused-parameter'], language: 'c')
# add_project_arguments(['-fanalyzer'], language: 'c')

curl_dep = dependency(
	'libcurl', 
	not_found_message: 'libcurl not found! Please install libcurl4-openssl-dev or the appropriate package for your system.'
)

zmq_dep = dependency(
	'libzmq',
	not_found_message: 'libzmq not found! Please install libzmq3-dev or the appropriate package for your system.'
)

csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true).as_link_whole()
slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: true).as_link_whole()
param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true).as_link_whole()
apm_api_dep = dependency(
	'apm',
	fallback: 
	[
		'apm', 
		'apm_csh_dep'
	],
	required: true).partial_dependency(
		links: false, 
		includes: true
	)

csh_sources = [
	'src/main.c',
	'src/slash_apm.c',
	'src/slash_csp.c',
	'src/slash_eth.c',
	'src/csp_debug_param.c',
	'src/base16.c',
	'src/param_sniffer.c',
	'src/prometheus.c',
	'src/resbuf_dump.c',
	'src/stdbuf_mon.c',
	'src/stdbuf_client.c',
	'src/randombytes.c',
	'src/csp_scan.c',
	'src/sleep_slash.c',
	'src/spaceboot_slash.c',
	'src/nav.c',
	'src/serial.c',
	'src/hk_param_sniffer.c',
	'src/lock.c',
	'src/csp_init_cmd.c',
	'src/vts.c',
	'src/walkdir.c',
    'src/victoria_metrics.c', 
    'src/loki.c',
    'src/slash_env_var_cmds.c', 
    'src/slash_nodes_cmds.c', 
    'src/slash_run_environment.c', 
    'src/require_version_cmd.c', 
	'src/param_commands_slash.c',
	'src/param_scheduler_slash.c',
	'src/param_list_slash.c',
	'src/param_slash.c',
	'src/vmem_client_slash.c',
	'src/vmem_client_slash_ftp.c',
	'src/param_scheduler_client.c',
	'src/param_commands_client.c'
]

python_dep = dependency('python3', version : '>=3.10', required: false)

if python_dep.found()
	py = import('python').find_installation('python3')
	python_ldflags = run_command('python'+py.language_version()+'-config', '--ldflags', '--embed', check: true).stdout().strip().split()
	python_ldflags += ['-DHAVE_PYTHON']
	pycsh_dep = dependency('pycsh', fallback: ['pycsh', 'pycsh_dep']).as_link_whole()
	csh_sources += [
		'src/python/python_loader.c'
	]
else
	python_ldflags = []
	pycsh_dep = []
endif

csh_sources += vcs_tag(input: files('src/version.c.in'), output: 'version.c', command: ['git', 'describe', '--always', '--dirty=+'])

utils_lib_src = [
	'src/url_utils.c',
	'src/environment.c',
	'src/require_version.c',
	'src/csh_defaults.c',
	'src/slash_utils.c',
	'src/known_hosts.c'
]
utils_lib_inc = include_directories('src')
utils_lib = static_library('csh_utils', utils_lib_src, include_directories: utils_lib_inc, dependencies: [apm_api_dep])
utils_lib_dep = declare_dependency(
	include_directories: utils_lib_inc,
	link_with: utils_lib,
	dependencies: [apm_api_dep]
)

subdir('tests')

csh = executable('csh', csh_sources,
	dependencies : [
		python_dep,
		apm_api_dep,
		slash_dep,
		csp_dep,
		param_dep,
		curl_dep,
		utils_lib_dep.as_link_whole(),
		pycsh_dep
	],
	link_args : python_ldflags + ['-Wl,-Map=csh.map', '-lm', '-Wl,--export-dynamic', '-ldl'],  # -ldl is needed on ARM/raspbarian
	install : true,
)

custom_target('size', output: ['dummy.txt'], command: [find_program('size'), csh.full_path()], depends: csh, build_by_default: true)

zmqproxy_sources = ['src/zmqproxy.c']
zmqproxy = executable('zmqproxy', zmqproxy_sources,
	dependencies : [csp_dep, zmq_dep],
	install : true,
)

spacebridge_sources = ['src/spacebridge.c']
spacebridge = executable('spacebridge', spacebridge_sources,
	dependencies : [csp_dep],
	install : true,
)

install_data('init/caninit', install_dir : get_option('bindir'))
