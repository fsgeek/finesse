#!/usr/bin/python

import os
import sys
import argparse
import pathlib
import shutil
import subprocess
import time
import json
import atexit
import tempfile


# Sample bitbucket run
#  tony@wam-desktop:~/projects/finesse/build/gcc/release$ finesse/bitbucket/bitbucket /mnt/bitbucket -o allow_root -o auto_unmount --logfile=/home/tony/bblog-2020-08-17-13:27.log --loglevel=3

# Sample filebench run
#  filebench -f fileserver.f

# Sample filebench + finesse run
# LD_PRELOAD=/home/tony/projects/finesse/build/gcc/debugoptimized/finesse/preload/libfinesse_preload.so filebench -f fileserver.f

'''
Testing Finesse:
    * Start by ensuring that the mountpoint is not in use (e.g., we're not already mounted on it)
    * Run the base filebench run _on the native file system_
    * Mount the test file system
    * Run the base filebench run _on the test file system_
    * Dismount the test file system
    * Run the finesse enhanced filebench run _on the test file system_
    * Dismount the test file system

Should set this up to run multiple times.
'''


def retrieve_git_hash(dir):
    '''For the given directory, retrieve the current git hash'''
    results = subprocess.run(
        ['git', 'rev-parse', 'HEAD'], cwd=dir, capture_output=True)
    if type(results.stdout) is bytes:
        return results.stdout.decode('ascii').strip()
    else:
        return results.stdout.strip()


class Filebench:

    target_workload = 'fileserver.f'
    base_search_dir = '/'
    default_cache_file = '~/.finesse_test_fbdata.json'
    ld_debug_options = ('libs',
                        'reloc',
                        'symbols',
                        'bindings',
                        'versions',
                        'scopes',
                        'all',
                        'statistics',
                        'unused',
                        )
    default_test_dir = '/mnt/bitbucket'

    def __init__(self, logfile=None):
        '''Set up a filebench run instance, specify a log file (for filebench). Other options can be set up separately if defaults are not wanted'''
        self.default_script = self.target_workload
        self.cache_file = os.path.expanduser(self.default_cache_file)
        self.load_workload()
        self.workload_dir = self.workload_dirs[-1]
        self.temp_dir = tempfile.mkdtemp()
        self.test_dir = self.default_test_dir
        atexit.register(self.cleanup)
        self.setup_done = False
        self.preload = None
        self.preload_debug_options = {x: False for x in self.ld_debug_options}
        self.preload_debug_log = None
        self.savetemp = True  # False
        self.logfile = logfile
        self.find_scripts()
        # Log creation is deferred

    def find_scripts(self):
        # only scripts with the 'run' keyword in them are top level; others are
        # used by inclusion
        if not hasattr(self, 'scripts'):
            results = subprocess.run(
                ['grep', '-r', '-l', 'run', '.'], capture_output=True, cwd=self.workload_dir)
            assert results.returncode == 0, 'grep failed'
            self.scripts = [t[2:-2]
                            for t in results.stdout.decode('ascii').split()]
        return self.scripts

    def find_workloads(self, start_dir='/', workload='fileserver.f', clean=False):
        '''Given a starting directory, find all the locations where the given workload file is present'''
        if hasattr(self, 'fblist'):  # if we've already loaded it, no need to do so again
            return self.fblist
        if self.cache_file:
            if '~' in self.cache_file:
                fbdata_file = os.path.expanduser(self.cache_file)
            else:
                fbdata_file = self.cache_file
        if os.path.exists(fbdata_file):
            if clean:
                os.unlink(fbdata_file)
            else:
                print('Using existing filebench location data')
                with open(fbdata_file, 'rt') as fd:
                    self.fblist = json.load(fd)
                return self.fblist
        print('Starting search for filebench workloads')
        results = subprocess.run(
            ['find', start_dir, '-name', workload], capture_output=True)
        # we can't check the return code because every "access denied" in the search is considered a failure
        # results.check_returncode()
        if results.stdout is None or len(results.stdout) == 0:
            # empty list
            fblist = None
        elif type(results.stdout) is bytes:
            fblist = results.stdout.decode('ascii')
        else:
            fblist = results.stdout
        fblist = [os.path.dirname(x) for x in fblist.split() if 'tmp' not in x]
        with open(fbdata_file, 'wt') as fd:
            json.dump(fblist, fd, ensure_ascii=True, indent=4)
        print('Finished search for filebench workloads, found {}'.format(fblist))
        self.fblist = fblist
        return self.fblist

    def cleanup(self):
        if self.savetemp:
            # Use this to preserve the temp directory (good for debugging)
            return
        if os.path.exists(self.temp_dir):
            shutil.rmtree(self.temp_dir)

    def set_test_dir(self, test_dir):
        self.test_dir = test_dir

    def set_savetemp(self, save=False):
        self.savetemp = save

    def get_workload_dir(self):
        '''Returns the workload directory being used'''
        return self.workload_dir

    def set_workload_dir(self, dir):
        '''Changes the workload directory being used'''
        assert dir in self.workload_dirs, 'Unknown workload directory'
        self.workload_dir = dir
        return self.workload_dir

    def get_workload_dirs(self):
        '''Return a list of the known (and acceptable) workload directories'''
        return self.workload_dirs

    def get_default_script(self):
        '''Return the name of the script to be run'''
        return self.default_script

    def set_default_script(self, script):
        '''Change the name of the script to be run; if the .f is omitted, it is added'''
        if len(script) > 2 and '.f' != script[-2:]:
            script = script + '.f'
        assert os.path.exists('{}/{}'.format(self.workload_dir, script))
        self.default_script = script

    def load_workload(self):
        '''Since it takes a while to find where workload(s) are stored, we cache them.  This loads them from the cache'''
        if os.path.exists(self.cache_file):
            with open(self.cache_file, 'rt') as fd:
                try:
                    self.workload_dirs = json.load(fd)
                    return self.workload_dirs
                except Exception as e:
                    print('Cache file {} raised exception {}'.format(
                        self.cache_file, e))
                    os.unlink(self.cache_file)
        self.build_workload_cache()
        return self.workload_dirs

    def delete_workload_cache(self):
        '''This deletes the workload cache'''
        if os.path.exists(self.cache_file):
            os.unlink(self.cache_file)
        self.workload_dirs = []
        return self.workload_dirs

    def build_workload_cache(self):
        '''Compute the list of locations where workload files are located and cache the results'''
        if os.path.exists(self.cache_file):
            return self.load_workload()
        print('Starting search for filebench workloads (this may take a while)')
        results = subprocess.run(
            ['find', self.base_search_dir, '-name', self.target_workload], capture_output=True)
        if results.stdout is None or len(results.stdout) == 0:
            # empty
            filebench_list = []
        elif type(results.stdout) is bytes:
            filebench_list = results.stdout.decode('ascii')
        else:
            filebench_list = results.stdout
        # omit any names with tmp in them; this excludes /tmp, but could exclude things we don't want to exclude
        self.workload_dirs = [os.path.dirname(
            x) for x in filebench_list.split() if 'tmp' not in x]
        with open(self.cache_file, 'wt') as fd:
            json.dump(self.workload_dirs, fd, ensure_ascii=True, indent=4)
        print('Finished search for filebench workloads')
        return self.workload_dirs

    def set_preload(self, preload_library, debug_options=[], debug_log=None):
        '''
        Set LD_PRELOAD options.
        For a more comprehensive list (this is a subset) see https://man7.org/linux/man-pages/man8/ld.so.8.html
        '''
        if '~' in preload_library:
            preload_library = os.path.expanduser(preload_library)
        if preload_library[0].startswith('./'):
            # ./foo/bar/preload.so -> foo/bar/preload.so
            preload_library = '{}/{}'.format(os.getcwd(), preload_library[2:])
        elif '/' != preload_library[0]:
            preload_library = '{}/{}'.format(os.getcwd(), preload_library)
        assert os.path.exists(
            preload_library), 'Unable to find preload library {}'.format(preload_library)
        self.preload = {'LD_PRELOAD': preload_library}
        for do in debug_options:
            self.preload['LD_DEBUG': ','.join(debug_options)]
        if self.preload_debug_log != None:
            if '~' in self.preload_debug_log:
                self.preload_debug_log = os.path.expanduser(
                    self.preload_debug_log)
            self.preload['LD_DEBUG_OUTPUT'] = self.preload_debug_log

    def get_preload(self, debug=False):
        # ensures that if self.preload is None we still get a dict
        if type(self.preload) is dict:
            preload = self.preload
        else:
            preload = {}
        preload_debug_options = ""
        for do in self.preload_debug_options:
            if self.preload_debug_options[do]:
                preload_debug_options = preload_debug_options + do + ","
        if len(preload_debug_options) > 0:
            # remove trailing comma
            preload_debug_options = preload_debug_options[:-1]
            preload['LD_DEBUG'] = preload_debug_options
        if self.preload_debug_log:
            preload['LD_DEBUG_OUTPUT'] = self.preload_debug_log
        return preload

    def generate_script(self, timestamp):
        '''
        Because we run things with SUDO, we need to run them within a script so we can set the environment
        and have it stick.  Otherwise, our LD_PRELOAD settings won't be properly preserved.
        '''
        args = ['filebench', '-f',
                '{}/{}'.format(self.temp_dir, self.default_script)]
        script = '{}/finesse_test.sh'.format(self.temp_dir)
        with open(script, 'wt') as fd:
            fd.write('#!/bin/bash\n')
            env = self.get_preload()
            for item in env:
                if 'LD' not in item:  # don't care about non LD_XXX values
                    continue
                fd.write('export {}={}\n'.format(item, env[item]))
            fd.write('\n')
            fd.write(' '.join(args) + '\n')
        return script

    def setup_tests(self):
        '''This creates the temporary workload directory, copies the workload, and modifies it'''
        target = '$dir='
        target_length = len(target)
        assert os.path.isdir(self.temp_dir)
        filebench_files = [f for f in os.listdir(
            self.workload_dir) if len(f) > 2 and f[-2:] == '.f']
        for f in filebench_files:
            with open('{}/{}'.format(self.workload_dir, f), 'rt') as fd:
                lines = fd.readlines()
            with open('{}/{}'.format(self.temp_dir, f), 'wt') as fd:
                for line in lines:
                    if target in line:
                        line = line[:line.index(
                            target) + target_length] + self.test_dir + '\n'
                    fd.write(line)
        self.setup_done = True

    def get_log(self):
        '''This creates the log file'''
        if not hasattr(self, 'log'):
            self.setup_log()
        return self.log

    def set_log(self, log):
        '''Set the log for this object to use'''
        self.log = log

    def setup_log(self):
        if hasattr(self, 'log'):
            return  # nothing to do
        if self.logfile is None:
            # generate a default name
            self.logfile = './finesse_test_results-{}.log'.format(
                time.strftime('%Y%m%d-%H%M%S'))
        self.log = open(self.logfile, 'wt')

    def run(self, timestamp, run_as_root=True):
        '''This will run the default script and write to the specified log file (or log desciptor); an optional preload library may be specified'''
        if not self.setup_done:
            self.setup_tests()
        if run_as_root and 0 != os.geteuid():
            args = ['sudo']
        else:
            args = []
        args.append('/bin/bash')
        args.append(self.generate_script(timestamp))
        result = subprocess.run(args, stdout=self.get_log(),
                                stderr=subprocess.STDOUT)
        return result.returncode


class Bitbucket:

    __default_cache_file__ = '~/.finesse_test_bbdata.json'
    __default_log_level__ = 3
    __mountpoint_name__ = '/mnt/bitbucket'
    __mount_options__ = ['-o',  'allow_root',  '-o',  'auto_unmount']
    # finesse/bitbucket/bitbucket / mnt/bitbucket - o allow_root - o auto_unmount - -logfile = /home/tony/bblog-2020-08-17-13: 27.log - -loglevel = 3

    def __init__(self):
        self.mountpoint_name = self.__mountpoint_name__
        self.mount_options = self.__mount_options__
        self.timestamp = time.strftime('%Y%m%d-%H%M%S')
        self.bblog_level = self.__default_log_level__
        self.bb_log = 'bblog-{}-{}.log'.format(
            self.timestamp, self.bblog_level)
        self.bb_log_dir = '.'
        self.log = sys.stdout
        self.cache_file = self.__default_cache_file__
        if '~' in self.cache_file:
            self.cache_file = os.path.expanduser(
                self.cache_file)

    def set_program(self, bbfs):
        '''Set the location of the bitbucket file system binary to use'''
        self.bbfs = bbfs

    def get_program_args(self):
        assert hasattr(
            self, 'bbfs'), 'Must set the correct binary before invoking'
        args = [self.bbfs, self.mountpoint_name] + self.mount_options
        if self.bb_log != None:
            args.append('--logfile={}/{}'.format(self.bb_log_dir, self.bb_log))
            args.append('--loglevel={}'.format(self.bblog_level))
        return args

    def get_log(self):
        '''This retrieves the current log file for this script'''
        return self.log

    def set_log(self, log):
        '''This sets the current log file to use for this script'''
        self.log = log

    def get_bblog(self):
        '''This returns the name of the current log file being used by bitbucket'''
        return self.bb_log

    def set_bblog(self, log):
        '''Set the name that should be used for the log file passed to bitbucket'''
        self.bb_log = log

    def get_bblog_dir(self):
        return self.bb_log_dir

    def set_bblog_dir(self, dir):
        self.bb_log_dir = dir

    def get_bblog_level(self):
        return self.bblog_level

    def set_bblog_level(self, level):
        return self.bblog_level

    def get_program(self):
        return ' '.join(self.get_program_args())

    def get_mountpoint(self):
        return self.mountpoint_name

    def is_mounted(self):
        result = subprocess.run(['mount'], capture_output=True)
        result.check_returncode()
        return self.mountpoint_name in result.stdout.decode('ascii')

    def mount(self):
        assert not self.is_mounted(), 'Bitbucket is already mounted!'
        self.get_log().write('*** MOUNT ***\n')
        self.get_log().write(
            '*** COMMAND: {} ***\n'.format(self.get_program()))
        result = subprocess.run(
            self.get_program_args(), stdout=self.get_log(), stderr=subprocess.STDOUT)
        if result.returncode != 0:
            self.get_log().write(
                '*** MOUNT FAILED ({}) ***'.format(result.returncode))
        return result.returncode

    def umount(self):
        assert self.is_mounted(), 'Bitbucket is not mounted!'
        result = subprocess.run(
            ['umount', self.__mountpoint_name__], capture_output=True)
        if result.returncode != 0:
            print(result.stdout.decode('ascii'),
                  result.stderr.decode('ascii'))
        return result.returncode

    def find_default_build_dir(self):
        '''Given a list of build directories, use the one that indicates it is using gcc in release mode'''
        default = None
        for bd in self.bd_list:
            if 'gcc' in bd and 'release' in bd:
                default = bd
                break
        return default

    def find_build_dirs(self, start_dir='.', clean=False):
        '''Given a starting directory, find all the locations where we can find builds of Finesse'''
        dirs = self.__find_build_dirs_internal__(
            start_dir=start_dir, clean=clean)
        return (self.find_default_build_dir(), self.bd_list)

    def __find_build_dirs_internal__(self, start_dir, clean):
        '''Internal work routine that ensures we have a build directory list'''
        if hasattr(self, 'bd_list'):  # if we already loaded it, just return
            return
        if clean and os.path.exists(self.cache_file):
            os.unlink(self.cache_file)
        if os.path.exists(self.cache_file):
            with open(self.cache_file, 'rt') as fd:
                try:
                    self.bd_list = json.load(fd)
                except Exception as e:
                    print('Loading {} failed ({}), rebuilding'.format(
                        self.cache_file, e))
        if hasattr(self, 'bd_list'):
            return
        print('Starting search for Finesse builds')
        results = subprocess.run(
            ['find', start_dir, '-name', 'libfinesse_preload.so'], capture_output=True)
        results.check_returncode()
        if type(results.stdout) is bytes:
            bdlist = results.stdout.decode('ascii')
        else:
            bdlist = results.stdout
        self.bd_list = []
        for e in [os.path.dirname(x) for x in bdlist.split()]:
            parts = e.split('/')
            self.bd_list.append('/'.join(parts[:-2]))
        print('Finished searching for Finesse builds')


def run(build_dir, data_dir, tests, bitbucket, fb, trial):
    assert type(tests) == list, 'Tests is expected to be a list'
    assert len(tests) > 0, 'Tests is expected to be a non-empty list'
    # Use the same timestamp for all test runs within a single group
    timestamp = time.strftime('%Y%m%d-%H%M%S')

    # I need two filebench objects: one that runs with LD_PRELOAD
    # (for finesse) and one that runs without.  But I want it all
    # in a single log file
    bitbucket.set_program(build_dir + '/finesse/bitbucket/bitbucket')

    # Cleanup (from prior run?)
    while bitbucket.is_mounted():
        bitbucket.umount()

    # Make sure that we don't have any detritus left over from prior usage of the
    # (unmounted) directory.
    if os.path.exists(bitbucket.get_mountpoint()):
        result = subprocess.run(
            ['sudo', 'rm', '-rf', bitbucket.get_mountpoint()])
        assert result.returncode == 0, 'Cleaning up {} failed {}'.format(
            bitbucket.get_mountpoint(), result.returncode)
        result = subprocess.run(
            ['sudo', 'mkdir', '-p', bitbucket.get_mountpoint()])
        assert result.returncode == 0, 'Making dir {} failed {}'.format(
            bitbucket.get_mountpoint(), result.returncode)
        result = subprocess.run(
            ['sudo', 'chown', str(os.getuid()), bitbucket.get_mountpoint()])
        assert result.returncode == 0, 'Changing owner of {} to {} failed {}'.format(
            bitbucket.get_mountpoint(), os.getuid(), result.returncode)

    preload_fb = Filebench()
    preload_fb.set_preload(
        build_dir + '/finesse/preload/libfinesse_preload.so')

    for test in tests:
        # Let's make sure everything is clean
        print('Starting test {}'.format(test))
        logfile = '{}/finesse_test#{}#results#{}.log'.format(
            data_dir, test, timestamp)
        with open(logfile, 'wt+') as fd:
            # (0) Write preamble information
            fd.write('Finesse Test Data Collection Run: {}\n'.format(timestamp))
            fd.write('Git Hash: {}\n'.format(retrieve_git_hash(build_dir)))
            fd.flush()
            subprocess.run(['mount'], stdout=fd, stderr=subprocess.STDOUT)
            fd.write('\nRun: Test {} on native file system\n'.format(test))
            fd.flush()
            # (1) Run on native file system
            fb.set_log(fd)
            if trial:
                fd.write("This is where we'd run the test")
            else:
                fb.run(timestamp)
            fd.write('\nEnd Run\n')

            # (2) Run on Bitbucket
            fd.write('Mount bitbucket: {}\n'.format(bitbucket.get_program()))
            fd.flush()
            bitbucket.set_log(fd)
            bitbucket.set_bblog(
                '{}/bblog#{}#data#{}.log'.format(data_dir, test, timestamp))
            bitbucket.mount()
            fd.write('Run: Test {} on bitbucket file system\n'.format(test))
            fd.flush()
            if trial:
                fd.write("This is where we'd run the test")
            else:
                fb.run(timestamp)
            bitbucket.umount()
            fd.write('\nEnd Run\n')

            # (3) Run on Bitbucket with LD_PRELOAD (finesse) library
            preload_fb.set_log(fd)
            bitbucket.set_bblog(
                '{}/bblog-preload#{}#data#{}.log'.format(data_dir, test, timestamp))
            bitbucket.mount()
            fd.write(
                'Run: Test {} on bitbucket file system with LD_PRELOAD (for filebench)\n'.format(test))
            fd.flush()
            if trial:
                fd.write("This is where we'd run the test (LD_PRELOAD={}".format(
                    preload_fb.get_env()['LD_PRELOAD']))
            else:
                preload_fb.run(timestamp)
            try:
                bitbucket.umount()
            except Exception as e:
                fd.write('Final dismount failed\n{}\n'.format(e))
            fd.write('\nEnd Run\n')

            # Postamble
            fd.write('Completed run {} at time {}'.format(
                timestamp, time.strftime('%Y%m%d-%H%M%S')))

            # Cleanup
            while bitbucket.is_mounted():
                bitbucket.umount()


def main():
    bitbucket = Bitbucket()
    default_build_dir, build_dirs = bitbucket.find_build_dirs()
    fb = Filebench()
    workload_dirs = fb.find_workloads()
    if workload_dirs == None or len(workload_dirs) == 0:
        print('Could not find filebench workload(s).  Please install filebench.')
        return -1
    assert 'fileserver' in fb.find_scripts(
    ), 'Default option fileserver not found in scripts'
    parser = argparse.ArgumentParser(
        description='Test Finesse+Bitbucket with Filebench')
    parser.add_argument('--testdir', dest='test_dir',
                        default=workload_dirs[-1], choices=workload_dirs, help='Where to find filebench workloads')
    parser.add_argument('--test', dest='test', default=['all'],
                        choices=fb.find_scripts() + ['all'], help='Which filebench test to run')
    parser.add_argument('--build_dir', dest='build_dir',
                        default=default_build_dir, choices=build_dirs, help='Which build to use')
    parser.add_argument('--datadir', dest='data_dir',
                        default='data', help='Where to store the results')
    parser.add_argument('--mountpoint', dest='mountpoint',
                        default='/mnt/bitbucket', help='name of mount point to use')
    parser.add_argument('--runs', dest='runs', default=1,
                        type=int, help='Number of runs to perform')
    parser.add_argument('--clean', dest='clean', default=False, action='store_true',
                        help='Indicates if saved state should be discarded and rebuilt')
    parser.add_argument('--trial', dest='trial', default=False,
                        action='store_true', help='Indicate that this should be a trial run')
    args = parser.parse_args()
    if args.clean:
        default_build_dir, build_dirs = bitbucket.find_build_dirs(clean=True)
        workload_dirs = fb.find_workloads(clean=True)
        args = parser.parse_args()

    # Handle the 'all' case for the tests
    if type(args.test) == str:
        args.test = [args.test]
    assert type(args.test) == list, 'Expected tests to be a list, not {}'.format(
        type(args.test))
    if len(args.test) == 1 and 'all' == args.test[0]:
        args.test = fb.find_scripts()

    # Invoke the run logic
    run(args.build_dir, args.data_dir, args.test, bitbucket, fb, args.trial)


if __name__ == "__main__":
    main()
