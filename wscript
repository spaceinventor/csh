#!/usr/bin/env python
# encoding: utf-8

from waftools import eclipse

APPNAME = 'satctl'
VERSION = '0.3.0'

top = '.'
out = 'build'

modules = ['lib/csp', 'lib/slash', 'lib/satlab', 'lib/param']

def options(ctx):
    ctx.load('eclipse')
    ctx.recurse(modules)

def configure(ctx):
    ctx.load('eclipse')

    # CSP options
    ctx.options.disable_stlib = True
    ctx.options.enable_if_can = True
    ctx.options.enable_can_socketcan = True

    ctx.recurse(modules)

def build(ctx):
    ctx.recurse(modules)
    ctx.program(
        target   = APPNAME,
        source   = ctx.path.ant_glob('src/*.c'),
        use      = ['csp', 'slash', 'satlab', 'param'],
        defines  = ['SATCTL_VERSION="%s"' % VERSION],
        lib      = ['pthread'] + ctx.env.LIBS)

def dist(ctx):
    ctx.algo      = 'tar.xz'
    ctx.excl      = '**/.* build'
