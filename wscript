#!/usr/bin/env python

VERSION = '0.0.1'
APPNAME = 'hpy_lda'

def options(opt):
    opt.load('compiler_cxx')
    opt.load('unittest_gtest')

def configure(conf):
    conf.load('compiler_cxx')
    #conf.check_cxx(lib = ['pficommon', 'pficommon_data', 'pficommon_text', 'pficommon_system'], uselib_store = 'PFICOMMON')
    conf.check_cfg(package = 'pficommon', args = '--cflags --libs', atleast_version = '1.2.10')

    conf.check_cxx(lib = 'pthread')
    if conf.env.CXX == ['clang++']:
        conf.load('unittest_gtest')
        conf.env.append_unique(
            'CXXFLAGS',
            ['-std=c++0x', '-stdlib=libc++', '-g', '-Wall', '-Wextra', '-O2']
            #['-std=c++0x', '-g', '-Wall', '-Wextra', '-O2']
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

