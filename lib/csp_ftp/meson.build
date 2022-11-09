project('csp_ftp', 'c')

csp_dep = dependency('csp', fallback : ['csp', 'csp_dep'])

csp_ftp_src = files([
	'src/ftp_client.c',
	'src/ftp_server.c',
])

csp_ftp_inc = include_directories('include')

csp_ftp_lib = static_library('csp_ftp',
	sources: [csp_ftp_src],
	include_directories : csp_ftp_inc,
	dependencies : [csp_dep],
	install : false
)

csp_ftp_dep = declare_dependency(include_directories : csp_ftp_inc, link_with : csp_ftp_lib)
