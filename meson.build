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
	'src/slash_csp.c',
	'src/base16.c',
	'src/rewl_test.c',
]

ldscript = files('linux.ld')

satctl = executable('satctl', satctl_sources,
	dependencies : [slash_dep, csp_dep, param_dep],
	link_args : ['-Wl,-Map=satctl.map', '-lm', '-T@0@/@1@'.format(meson.current_source_dir(), ldscript[0])],
        link_depends : ldscript,
	install : true,
)

custom_target('size', output: ['dummy.txt'], command: [find_program('size'), satctl.full_path()], depends: satctl, build_by_default: true)
