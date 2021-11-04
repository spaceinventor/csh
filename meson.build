project('satctl', 'c', subproject_dir: 'lib', default_options: [
	'buildtype=release', 
	'c_std=gnu11', 
	'b_lto=false', 
	'csp:packet_padding_bytes=42', 
	'csp:buffer_count=50',
	'csp:buffer_size=2048',
	'csp:conn_max=20',
	'csp:conn_rxqueue_len=20',
	'csp:qfifo_len=20',
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

csp = subproject('csp')
csp_dep = csp.get_variable('csp_dep')

slash = subproject('slash')
slash_dep = slash.get_variable('slash_dep')

param = subproject('param')
param_dep = param.get_variable('param_dep_whole')

satctl_sources = [
	'src/satctl.c',
	'src/time.c',
	'src/slash_csp.c',
	'src/csp_debug_param.c',
	'src/csp_param.c',
	'src/base16.c',
	'src/param_sniffer.c',
	'src/tweetnacl.c',
	'src/crypto.c',
	'src/crypto_param.c',
	'src/crypto_test_slash.c',
	'src/prometheus.c',
	'src/stdbuf_mon.c',
	'src/csp_if_tun.c',
	'src/randombytes.c',
	'src/tfetch_once.c',
	'src/csp_if_eth.c',
	'src/it6942a.c',
	'src/csp_scan.c',
]

satctl = executable('satctl', satctl_sources,
	dependencies : [slash_dep, csp_dep, param_dep],
	link_args : ['-Wl,-Map=satctl.map', '-lm'],
	install : true,
)

custom_target('size', output: ['dummy.txt'], command: [find_program('size'), satctl.full_path()], depends: satctl, build_by_default: true)

zmqproxy_sources = ['src/zmqproxy.c']
zmqproxy = executable('zmqproxy', zmqproxy_sources,
	dependencies : [csp_dep],
	install : true,
)
