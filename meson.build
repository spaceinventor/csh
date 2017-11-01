project('satctl', 'c', subproject_dir: 'lib', default_options: ['buildtype=release', 'c_std=gnu99', 'b_lto=false'])

csp = subproject('csp')
csp_dep = csp.get_variable('csp_dep')

slash = subproject('slash')
slash_dep = slash.get_variable('slash_dep')

param = subproject('param')
param_dep = param.get_variable('param_dep')

satctl_sources = [
	'src/satctl.c',
	'src/time.c',
	'lib/si/src/slash/slash_csp.c',
	'lib/si/src/slash/base16.c',
]

# Provoke some undefined symbols in order to prevent 
# garbage collection of slash commands
# Only one command is needed from each object file.
slash_cmd = [
	'-uslash_cmd_info',     # slash_csp.c
	'-uslash_cmd_pull',     # param_client_slash.c
	'-uslash_cmd_get',      # param_slash.c
	'-uslash_cmd_group',    # param_group_slash.c
	'-uslash_cmd_list',     # param_list_slash.c
	'-uslash_cmd_download', # vmem_client_slash_ftp.c
	'-uslash_cmd_vmem',     # vmem_client_slash.c
]

satctl = executable('satctl', satctl_sources,
	dependencies : [slash_dep, csp_dep, param_dep],
	link_args : ['-Wl,-Map=satctl.map', '-lm', '-T../linux.ld', slash_cmd],
	install : true,
)

custom_target('size', output: ['dummy.txt'], command: [find_program('size'), satctl.full_path()], depends: satctl, build_by_default: true)
