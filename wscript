#!/usr/bin/env python
# encoding: utf-8

APPNAME = 'satctl'
VERSION = '0.3.0'

top = '.'
out = 'build'

modules = ['lib/csp', 'lib/slash', 'lib/param']

def options(ctx):
    
    ctx.add_option('--chip', action='store', default='')
    
    ctx.recurse(modules)

def configure(ctx):

    # CSP options
    ctx.options.disable_stlib = True
    ctx.options.enable_if_can = True
    ctx.options.enable_can_socketcan = True
    ctx.options.enable_if_kiss = True
    ctx.options.with_driver_usart = 'linux'
    ctx.options.enable_crc32 = True
    ctx.options.enable_rdp = True
    
    ctx.options.slash_csp = True
    
    ctx.options.param_client = True
    ctx.options.param_client_slash = True
    ctx.options.param_server = True
    ctx.options.param_store_file = True
    ctx.options.param_group = True

    ctx.options.vmem_client = True
    ctx.options.vmem_client_ftp = True
    ctx.options.vmem_server = True
    ctx.options.vmem_ram = True
    ctx.options.vmem = True

    ctx.recurse(modules)
    
    ctx.env.prepend_value('CFLAGS', ['-Os', '-g', '-std=gnu99', '-D_FILE_OFFSET_BITS=64', '-DHAVE_TERMIOS_H'])

def build(ctx):
    ctx.recurse(modules)
    ctx.program(
        target   = APPNAME,
        source   = ctx.path.ant_glob('src/*.c'),
        use      = ['csp', 'slash', 'param', 'vmem'],
        defines  = ['SATCTL_VERSION="%s"' % VERSION, '_FILE_OFFSET_BITS=64'],
        lib      = ['pthread', 'm'] + ctx.env.LIBS,
        ldflags  = '-Wl,-Map=' + APPNAME + '.map')

def dist(ctx):
    ctx.algo      = 'tar.gz'
    ctx.excl      = '**/.* build'

    
