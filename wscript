#!/usr/bin/env python

VERSION = '0.0.1'
APPNAME = 'hpy_lda'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('unittest_gtest')
    opt.recurse('pficommon')

def configure(conf):
    conf.load('compiler_cxx')
    conf.recurse('pficommon')

    conf.check_cxx(lib = 'pthread')
    if conf.env.CXX == ['clang++']:
        conf.load('unittest_gtest')
        conf.env.append_unique(
            'CXXFLAGS',
            ['-std=c++0x', '-stdlib=libc++', '-g', '-Wall', '-Wextra', '-O2']
            )
        conf.env.append_unique(
            'LINKFLAGS',
            ['-lc++']
            )
    else:
        conf.load('unittest_gtest')
        conf.env.append_unique(
            'CXXFLAGS',
            ['-std=c++0x', '-Wall', '-g', '-Wextra', '-O2']
            )

def build(bld):
    bld.recurse('src')
    bld.recurse('pficommon')
