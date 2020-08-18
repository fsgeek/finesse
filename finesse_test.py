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


def find_workloads(start_dir='/', workload='fileserver.f', clean=False):
    '''Given a starting directory, find all the locations where the given workload file is present'''
    fbdata_file = './.finesse_test_fbdata.json'
    if os.path.exists(fbdata_file):
        if clean:
            os.unlink(fbdata_file)
        else:
            print('Using existing filebench location data')
            with open(fbdata_file, 'rt') as fd:
                fblist = json.load(fd)
            return fblist
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


def find_default_build_dir(build_dirs):
    '''Given a list of build directories, use the one that indicates it is using gcc in release mode'''
    default = None
    for bd in build_dirs:
        if 'gcc' in bd and 'release' in bd:
            default = bd
            break
    return default


def find_build_dirs(start_dir='.', clean=False):
    '''Given a starting directory, find all the locations where we can find builds of Finesse'''
    bddata_file = './.finesse_test_bddata.json'
    if os.path.exists(bddata_file):
        if clean:
            os.unlink(bddata_file)
        else:
            with open(bddata_file, 'rt') as fd:
                bd_list = json.load(fd)
            return (find_default_build_dir(bd_list), bd_list)
    print('Starting search for Finesse builds')
    results = subprocess.run(
        ['find', start_dir, '-name', 'libfinesse_preload.so'], capture_output=True)
    results.check_returncode()
    if type(results.stdout) is bytes:
        bdlist = results.stdout.decode('ascii')
    else:
        bdlist = results.stdout
    bd_list = []
    for e in [os.path.dirname(x) for x in bdlist.split()]:
        parts = e.split('/')
        bd_list.append('/'.join(parts[:-2]))
    default = None
    for e in bd_list:
        if 'gcc' in e and 'release' in e:
            default = e
            break
    with open(bddata_file, 'wt') as fd:
        json.dump(bd_list, fd, indent=4)
    print('Finished searcing for Finesse builds')
    return (default, bd_list)


def generate_timestamp():
    return time.strftime('%Y%m%d-%H%M%S')


def generate_log_file_name(timestamp=None, run=1):
    if timestamp == None:
        timestamp = generate_timestamp()
    return 'finesse-results-{}-{}.log'.format(timestamp, run)


def retrieve_git_hash(dir):
    '''For the given directory, retrieve the current git hash'''
    results = subprocess.run(
        ['git', 'rev-parse', 'HEAD'], cwd=dir, capture_output=True)
    if type(results.stdout) is bytes:
        return results.stdout.decode('ascii').strip()
    else:
        return results.stdout.strip()


def run_test(timestamp, test_args, iteration=1, logfile=None, details=None, env=None):
    '''
    This routine runs a test, records the results to a log file, and
    does so with clear delineations to make it easy(easier) to parse.
    '''
    timestamp = generate_timestamp()  # use the same timestamp for all test runs!
    if logfile == None:
        logfile = generate_log_file_name(timestamp, iteration)
    assert type(logfile) is str, 'Expected a string for the logfile name'
    with open(logfile, 'wt+') as log:
        log.write('*** Starting Pass {} ***\n'.format(iteration))
        if details == None:
            log.write('*** DETAILS ARE NONE ***\n')
        else:
            assert type(details) is str, 'Expected string for details'
            log.write('*** DETAILS ARE {} ***\n'.format(details))
        log.write('*** COMMAND is {} ***\n'.format(' '.join(args)))
        results = subprocess.run(
            test_args, stdout=log, stderr=subprocess.STDOUT, env=env)
        log.write(
            '*** COMMAND COMPLETION STATUS {} ***\n'.format(results.returncode))


class Filebench:

    target_workload = 'fileserver.f'
    base_search_dir = '/'
    default_cache_file = '~/.finesse_test_fbdata.json'

    def __init__(self):
        self.default_script = self.target_workload
        self.cache_file = os.path.expanduser(self.default_cache_file)
        self.workload_dirs = self.find_workloads(
            start_dir=self.base_search_dir, workload=self.target_workload, clean=False)
        self.workload_dir = self.workload_dirs[-1]
        self.tempdir = tempfile.mkdtemp()
        atexit.register(self.cleanup)

    def cleanup(self):
        if os.path.exists(self.tempdir):
            shutil.rmtree(self.tempdir)

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
        if not os.path.exists(self.cached_data):
            self.build_workload_cache()
        with open(self.cached_data, 'rt') as fd:
            self.workload_dirs = json.load(fd)
        return self.workload_dirs

    def delete_workload_cache(self):
        '''This deletes the workload cache'''
        if os.path.exists(self.cached_data):
            os.unlink(self.cached_data)
        self.workload_dirs = []
        return self.workload_dirs

    def build_workload_cache(self):
        '''Compute the list of locations where workload files are located and cache the results'''
        if os.path.exists(self.cached_data):
            return self.load_workload()
        print('Starting search for filebench workloads (this may take a while)')
        results = subprocess.run(
            ['find', self.base_search_dir, '-name', self.target_workload], capture_output=True)
        if results.stdout is None or len(results.stdout) == 0:
            # empty
            filebench_list = []
        elif types(results.stdout) is bytes:
            filebench_list = results.stdout.decode('ascii')
        else:
            filebench_list = results.stdout
        # omit any names with tmp in them; this excludes /tmp, but could exclude things we don't want to exclude
        self.workload_dirs = [os.path.dirname(
            x) for x in filebench_list.splot() if 'tmp' not in x]
        with open(self.cache_file, 'wt') as fd:
            json.dump(self.cache_file, ensure_ascii=True, indent=4)
        print('Finished search for filebench workloads')
        return self.workload_dirs

    def setup_tests(self):
        '''This creates the temporary workload directory, copies the workload, and modifies it'''
        target = '$dir='
        target_length = len(target)
        assert os.path.isdir(self.tempdir)
        filebench_files = [f for f in os.listdir(
            self.workload_dir) if len(f) > 2 and f[-2:] == '.f']
        for f in filebench_files:
            with open('{}/{}'.format(self.workload_dir, f), 'rt') as fd:
                lines = fd.readlines()
            with open('{}/{}'.format(self.tempdir, f), 'wt') as fd:
                for line in lines:
                    if target in line:
                        line = line[:line.index(
                            target) + target_length] + testdir + '\n'
                    fd.write(line)

    def run(self, log, preload=None, run_as_root=True):
        '''This will run the default script and write to the specified log file (or log desciptor); an optional preload library may be specified'''
        env = {x: os.environ[x] for x in os.environ}
        if preload != None:
            assert os.path.exists(
                preload), 'Preload {} does not exist'.format(preload)
            env['LD_PRELOAD'] = preload
        args = []
        if run_as_root and 0 != os.geteuid():
            args.append('sudo')
        args = args + ['filebench', '-f', self.default_script]
        result = self.call(args, env=env, stdout=log, stderr=subprocess.STDOUT)
        return result.returncode


class Bitbucket:

    __mountpoint_name__ = '/mnt/bitbucket'
    __mount_options__ = ['-o',  'allow_root',  '-o',  'auto_unmount']
    # finesse/bitbucket/bitbucket / mnt/bitbucket - o allow_root - o auto_unmount - -logfile = /home/tony/bblog-2020-08-17-13: 27.log - -loglevel = 3

    def __init__(self, bbfs, log_dir='.', log_level=3):
        assert not self.is_mounted(), 'Bitbucket is already mounted!'
        self.program = [bbfs, self.__mountpoint_name__] + \
            self.__mount_options__
        self.timestamp = time.strftime('%Y%m%d-%H%M%S')
        self.fs_logfile = '{}/bitbucket-{}.log'.format(log_dir, self.timestamp)
        self.program.append('--logfile={}'.format(self.fs_logfile))
        self.program.append('--loglevel={}'.format(log_level))
        self.test_logfile = '{}/filebench-{}.log'.format(
            log_dir, self.timestamp)
        self.fs_log = open(self.fs_logfile, 'wt+')
        self.test_log = open(self.test_logfile, 'wt+')

    def is_mounted(self):
        result = subprocess.run(['mount'], capture_output=True)
        result.check_returncode()
        return self.__mountpoint_name__ in result.stdout.decode('ascii')

    def mount(self):
        assert not self.is_mounted(), 'Bitbucket is already mounted!'
        self.fs_log.write('*** MOUNT ***\n')
        self.fs_log.write(
            '*** COMMAND: {} ***\n'.format(' '.join(self.program)))
        result = subprocess.run(
            self.program, stdout=self.fs_log, stderr=subprocess.STDOUT)
        if result.returncode != 0:
            self.fs_log.write(
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

    def run_filebench(self, workload_dir, test='fileserver.f'):
        '''
        Pass 1: ensure the file system isn't
        '''
        assert os.path.isdir(
            workload_dir), 'Workload directory does not appear to exist'


def main():
    default_build_dir, build_dirs = find_build_dirs()
    workload_dirs = find_workloads()
    if len(workload_dirs) == 0:
        print('Could not find filebench workload(s).  Please install filebench.')
        return -1
    parser = argparse.ArgumentParser(
        description='Test Finesse+Bitbucket with Filebench')
    parser.add_argument('--testdir', dest='testdir',
                        default=workload_dirs[-1], choices=workload_dirs, help='Where to find filebench workloads')
    parser.add_argument('--test', dest='test', default='fileserver',
                        choices=['fileserver'], help='Which filebench test to run')
    parser.add_argument('--build_dir', dest='build_dir',
                        default=default_build_dir, choices=build_dirs, help='Which build to use')
    parser.add_argument('--datadir', dest='datadir',
                        default='data', help='Where to store the results')
    parser.add_argument('--mountpoint', dest='mountpoint',
                        default='/mnt/bitbucket', help='name of mount point to use')
    parser.add_argument('--runs', dest='runs', default=1,
                        type=int, help='Number of runs to perform')
    parser.add_argument('--clean', dest='clean', default=False, action='store_true',
                        help='Indicates if saved state should be discarded and rebuilt')
    args = parser.parse_args()
    if args.clean:
        default_build_dir, build_dirs = find_build_dirs(clean=True)
        workload_dirs = find_workloads(clean=True)
    args = parser.parse_args()
    print(args)

    print(retrieve_git_hash(args.build_dir))

    bitbucket = Bitbucket(
        args.build_dir + '/finesse/bitbucket/bitbucket', log_level=6)

    print('Mounted returns: {}'.format(bitbucket.is_mounted()))
    bitbucket.mount()
    print('Mounted returns: {}'.format(bitbucket.is_mounted()))
    bitbucket.umount()
    print('Mounted returns: {}'.format(bitbucket.is_mounted()))


if __name__ == "__main__":
    main()
