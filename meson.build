project('satctl', 'c', subproject_dir: 'lib', default_options: ['buildtype=minsize', 'c_std=gnu99', 'b_lto=true'])

csp = subproject('csp')
csp_dep = csp.get_variable('csp_dep')

slash = subproject('slash')
slash_dep = slash.get_variable('slash_dep')

param = subproject('param')
param_dep = param.get_variable('param_dep')

satctl_sources = [
	'src/satctl.c',
	'src/time.c',
]

satctl = executable('satctl', satctl_sources,
	dependencies : [slash_dep, csp_dep, param_dep],
	link_whole : [slash.get_variable('slash'), param.get_variable('param')],
	link_args : ['-Wl,-Map=satctl.map'],
	install : true,
)

custom_target('size', output: ['dummy.txt'], command: [find_program('size'), satctl.full_path()], depends: satctl, build_by_default: true)
