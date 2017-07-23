project('satctl', 'c', subproject_dir: 'lib', default_options: ['buildtype=release', 'c_std=gnu99'])

libcsp = subproject('csp')
libcsp_dep = dependency('libcsp', fallback : ['csp', 'libcsp_dep'])

slash = subproject('slash')
slash_dep = dependency('slash', fallback : ['slash', 'slash_dep'])

satctl_sources = [
	'src/satctl.c'
]
	
executable('satctl', satctl_sources,
	dependencies : [slash_dep, libcsp_dep],
	link_whole : [slash.get_variable('slash')],
	install : true,
)