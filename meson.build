project('satctl', 'c', subproject_dir: 'lib', default_options: ['buildtype=release', 'c_std=gnu99'])

libcsp = subproject('csp', default_options : [
	'enable_driver_serial=true', 
	'enable_driver_socketcan=true',
	'enable_if_can=true'
])
libcsp_dep = libcsp.get_variable('libcsp_dep')

slash = subproject('slash')
slash_dep = slash.get_variable('slash_dep')

libparam = subproject('param')
libparam_dep = libparam.get_variable('libparam_dep')

satctl_sources = [
	'src/satctl.c',
	'src/time.c',
]

executable('satctl', satctl_sources,
	dependencies : [slash_dep, libcsp_dep, libparam_dep],
	link_whole : [slash.get_variable('slash'), libparam.get_variable('libparam')],
	link_args : ['-Wl,-Map=satctl.map'],
	install : true,
)