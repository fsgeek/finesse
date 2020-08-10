#!/usr/bin/python

import os
import sys
import argparse
import pathlib
import shutil
import subprocess


'''
Some notes from how I've been configuring things:

  307  CC=clang CXX=clang++ meson setup --buildtype release ../../..
  313  CC=clang CXX=clang++ meson setup --buildtype release ../../.. --warninglevel 3 --help
  314  CC=clang CXX=clang++ meson setup --buildtype release ../../.. --warnlevel 3 --werror --help
  315  CC=clang CXX=clang++ meson setup --buildtype release ../../.. --warnlevel 3 --werror
  325  CC=clang CXX=clang++ meson setup --buildtype release ../../.. --warnlevel 3 --werror
  451  CC=clang CXX=clang++ meson setup --buildtype debug ../../.. --warnlevel 3 --werror

To run gcc or clang with address sanitizer:
-ggdb -fsanitize=address -fno-omit-frame-pointer -static-libstdc++ -static-libasan -lrt

or leak sanitizer:
-ggdb -fsanitize=leak -fno-omit-frame-pointer -static-libstdc++ -static-liblsan -lrt

See: http://gavinchou.github.io/experience/summary/syntax/gcc-address-sanitizer/

'''

class BuildConfigData:
    cc_check = ('gcc', 'clang', 'icc', 'aocc')
    buildtypes = ('debug', 'debugoptimized', 'release')

    # https://github.com/mesonbuild/meson/blob/master/docs/markdown/Reference-tables.md#compiler-and-linker-selection-variables
    cc_env = {
        'gcc' : {'CC' : 'gcc', 'CXX' : 'g++'}, # use default linker
        'clang' : {'CC' : 'clang', 'CC_LD' : 'lld', 'CXX' : 'g++', 'CXX_LD' : 'lld'},
        'icc' : {'CC' : 'icc', 'CC_LD' : 'lld', 'CXX' : 'g++', 'CXX_LD' : 'ld'},
        'aocc' : {'CC' : 'aocc', 'CC_LD' : 'ld', 'CXX' : 'g++', 'CXX_LD' : 'ld'},
    }

    c_std = {
        'c89' : '-Dc_std=c89',
        'c99' : '-Dc_std=c99',
        'c11' : '-Dc_std=c11',
        'c18' : '-Dc_std=c18',
    }

    c_args = [
        '-D_GNU_SOURCE',
        '-DFINESSE',
    ]


class BuildConfig:
    buildtypes = BuildConfigData.buildtypes

    @staticmethod
    def compilers():
        return [x for x in BuildConfigData.cc_check if shutil.which(x) is not None]

    def __init__(self, base, compiler=BuildConfigData.cc_check[0], buildtype=BuildConfigData.buildtypes[0]):
        '''Given a base path, a compiler, and a build type, construct a configuration option object that
           can be used to control configuration and building of the specific configuration target'''
        self.compiler = compiler
        self.buildtype = buildtype
        self.base = base
        self.debug = False
        self.c_std = BuildConfigData.c_std['c18'] # default
        self.c_args = BuildConfigData.c_args # TODO: allow more selectivity here
        assert os.path.isdir(self.base), 'The base {} must be a valid directory'.format(base)
        self.dir = self.base + '/' + compiler + '/' + buildtype
        if not os.path.exists(self.dir): os.makedirs(self.dir)
        else: assert os.path.isdir(self.dir), 'The path specified {} is not a valid directory'

    def debug(self, debug=True):
        self.debug=debug

    def getoptions(self):
        '''
        Generate the meson options here - this is where we should add more customization for compilers,
        such as the profiler guided optimizations (gcc) or static analysis (clang).
        '''
        if self.buildtype == 'debug':
            options = ['-B_sanitize=address']
        elif self.buildtype == 'debugoptimized':
            options = ['-B_sanitize=address', '-Dc_args=-Og']
        else:
            options = ['-Dc_args=-ggdb']
        return options


    def fixpath(self, filter):
        '''Given a string, remove any path entry with that string within it.  Primary use case is to strip out intel
           paths if this is not icc - otherwise clang doesn't work.
        '''
        path = os.environ['PATH'].split(os.pathsep)
        cleanpath = [p for p in path if filter not in p]
        return os.pathsep.join(cleanpath)


    def getenv(self):
        '''
        Some meson settings are guided by environment variables, so this routine handles
        setting up the environment. for now, just tack on compiler options to the end.
        '''
        assert self.compiler in BuildConfigData.cc_env, 'Unsupported compiler'
        env = {}
        for field in os.environ:
            if self.compiler == 'icc':
                env[field] = os.environ[field]
                continue
            if field == 'PATH':
                env[field] = self.fixpath('intel')
                continue
            if 'intel' in os.environ[field]:
                continue # not icc, so we skip this value
            # default is to use the environment variable value
            env[field] = os.environ[field]
        for field in BuildConfigData.cc_env[self.compiler]:
            env[field] = BuildConfigData.cc_env[self.compiler][field]
        return env

    def setc_std(self, std):
        assert 'std' in BuildConfigData.c_std, 'Unknown C standard value'
        self.c_std = BuildConfig.c_std[std]

    def run(self, args, env=None):
        '''Given a list of arguments and an environment to use, run a program'''
        if env is None: env=self.getenv()
        assert os.path.isdir(self.dir), 'Target dir ({}) does not exist or is not a directory'.format(self.dir)
        results = subprocess.run(args, cwd=self.dir, check=False, env=env)
        if results.returncode != 0:
            print('Build for {} failed\n'.format(self.dir))
            if self.debug:
                print('Unable to run command {}'.format(args[0]))
                print('\t  args: {}'.format(args))
                print('\tresult: {}'.format(results.returncode))
                print('\t   env:')
                for field in env:
                    print('\t\t{}={}'.format(field, env[field]))
                print(self.dir, results)

    def buildfile(self):
        ninjafile = '{}/build.ninja'.format(self.dir)
        return os.path.exists(ninjafile)


    def setup(self):
        '''Set up the configuration properly for the given configuration'''
        if self.buildfile():
            print('ninjafile exists, skipping configuration')
            return
        srcdir = '../../..'
        args = ['meson', 'setup', '--buildtype', self.buildtype, '--warnlevel', '3', '--werror', self.c_std, srcdir]
        self.run(args)

    def build(self):
        '''Build the configuration'''
        if not self.buildfile():
            print('ninjafile does not exists, skipping build {}'.format(self.dir))
            return
        self.run(['ninja'])

    def clean(self):
        '''Clean the configuration'''
        if not self.buildfile():
            print('ninjafile does not exists, skipping build {}'.format(self.dir))
            return
        self.run(['ninja', 'clean'])


def main():
    compiler_choices = BuildConfig.compilers() + ['all']
    build_choices = ['debug', 'release', 'all']
    operation_choices = ['setup', 'build']
    parser = argparse.ArgumentParser()
    parser.add_argument('--operation', dest='operation', type=str, nargs=1,
                        choices=operation_choices, default=operation_choices[-1], help='set up or build')
    parser.add_argument('--compiler', dest='compiler', choices=compiler_choices,
                        default=compiler_choices[-1], help='compiler to use')
    parser.add_argument('--buildtype', dest='buildtype', choices=build_choices,
                        default=build_choices[-1], help='Type of build to perform')
    parser.add_argument('--dir', dest='build_dir', default='build', help='Location to use for building')
    parser.add_argument('--clean', dest='clean', default=False, action='store_true', help='Delete build directory if it already exists')
    args = parser.parse_args()

    # normalize the name (doesn't do variable substitution at present)
    if '~' in args.build_dir: args.build_dir=os.path.expanduser(args.build_dir)
    if args.build_dir[0] != '/': args.build_dir = os.getcwd() + '/' + args.build_dir

    # if we were asked to clean up first, and the build directory exists, delete it
    if args.clean and os.path.exists(args.build_dir):
        shutil.rmtree(args.build_dir)

    # if the build directory doesn't exist (for any reason), create it
    if not os.path.exists(args.build_dir):
        os.makedirs(args.build_dir)

    # if the user specified 'all' for compilers, we construct a list.  Otherwise we use the explicit complier they passed.
    # Note that the list of compiles is generated based upon what we could find on the system.
    if args.compiler == 'all':
        compilers = [x for x in BuildConfig.compilers()]
    else:
        compilers = [args.compiler]

    # meson supports many other build types, but this is enough for now
    if args.buildtype == 'all':
        builds = [x for x in BuildConfig.buildtypes]
    else:
        builds = [args.buildtype]

    # build a list of the configuration(s) to use
    configs = [BuildConfig(args.build_dir, x, y) for x in compilers for y in builds]

    for config in configs:
        try:
            config.setup()
            config.build()
        except Exception as e:
            print('exception {} for config {}'.format(e, config.compiler))
            pass


if __name__ == "__main__":
    main()
