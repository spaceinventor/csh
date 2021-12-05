project('csh', 'c', subproject_dir: 'lib', default_options: [
	'buildtype=release', 
	'c_std=gnu11', 
	'b_lto=false', 
	'csp:packet_padding_bytes=42', 
	'csp:buffer_count=100',
	'csp:buffer_size=2048',
	'csp:conn_max=40',
	'csp:conn_rxqueue_len=40',
	'csp:qfifo_len=40',
	'csp:port_max_bind=16',
	'param:have_fopen=true', 
	'param:slash=true', 
	'param:commands=false',
	'param:scheduler=false',
	'param:commands_client=true',
	'param:scheduler_client=true',
	'param:collector=true', 
	'param:list_dynamic=true'])

add_global_arguments('-I../include', language: 'c')

#csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true).as_link_whole()
csp_dep = dependency('csp', fallback: ['csp', 'csp_dep'], required: true)
slash_dep = dependency('slash', fallback: ['slash', 'slash_dep'], required: true).as_link_whole()
param_dep = dependency('param', fallback: ['param', 'param_dep'], required: true).as_link_whole()

csh_sources = [
	'src/main.c',
	#'src/time.c',
	'src/slash_csp.c',
	'src/csp_debug_param.c',
	'src/base16.c',
	'src/param_sniffer.c',
	'src/prometheus.c',
	'src/stdbuf_mon.c',
	'src/randombytes.c',
	'src/csp_if_eth.c',
	'src/it6942a.c',
	'src/csp_scan.c',
]

csh = executable('csh', csh_sources,
	dependencies : [slash_dep, csp_dep, param_dep],
	link_args : ['-Wl,-Map=csh.map', '-lm'],
	install : true,
)

custom_target('size', output: ['dummy.txt'], command: [find_program('size'), csh.full_path()], depends: csh, build_by_default: true)

zmqproxy_sources = ['src/zmqproxy.c']
zmqproxy = executable('zmqproxy', zmqproxy_sources,
	dependencies : [csp_dep],
	install : true,
)

spacebridge_sources = ['src/spacebridge.c']
spacebridge = executable('spacebridge', spacebridge_sources,
	dependencies : [csp_dep],
	install : true,
)
