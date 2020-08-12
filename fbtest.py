#!/usr/bin/python

import os
import sys
import argparse
import pathlib
import shutil
import subprocess
import tempfile
import time


def find_fb_help(dir):
    '''Given a directory, return a list of .f files and subdirectories'''
    candidates = [x for x in os.listdir(dir)]
    f_files = [f for f in candidates if not os.path.isdir(
        '{}/{}'.format(dir, f)) and len(f) > 2 and f[-2:] == '.f']
    dirs = [d for d in os.listdir(
        dir) if os.path.isdir('{}/{}'.format(dir, d))]
    return f_files, dirs


def find_filebench(startdir=os.path.expanduser('~')):
    fbdirs = []
    files, dirs = find_fb_help(startdir)
    if len(files) > 0:
        fbdirs.append(startdir)
    for dir in dirs:
        fbdirs = fbdirs + find_filebench('{}/{}'.format(startdir, dir))
    return fbdirs


def copy_tests(fbdir, testdir):
    '''
    Given a directory containing file bench tests, create a working directory,
    copy the tests from the filebench directory and fix them to use the
    specified test directory.
    '''
    target = '$dir='
    target_length = len(target)
    workdir = tempfile.mkdtemp()
    fbfiles = [f for f in os.listdir(fbdir) if len(f) > 2 and f[-2:] == '.f']
    for f in fbfiles:
        with open('{}/{}'.format(fbdir, f), 'r') as fd:
            lines = fd.readlines()
        with open('{}/{}'.format(workdir, f), 'w') as fd:
            for line in lines:
                if target in line:
                    line = line[:line.index(
                        target) + target_length] + testdir + '\n'
                fd.write(line)
    return workdir


def main():
    fbdirs = find_filebench(startdir=os.path.expanduser(
        '~/projects/filebench/workloads'))
    default_fbdir = None
    if len(fbdirs) > 0:
        default_fbdir = fbdirs[0]
    parser = argparse.ArgumentParser(
        description='Run filebench workload for a given file system')
    parser.add_argument('--wldir', dest='wldir', type=str, default=default_fbdir,
                        help='location of the base workload files')
    parser.add_argument('--testdir', dest='testdir', default='/mnt/bitbucket',
                        type=str, help='location of the test files')
    parser.add_argument('--test', dest='test', default='fileserver',
                        type=str, help='which filebench test to run')
    parser.add_argument('--log', dest='log', type=str,
                        help='name to use for log file (otherwise, auto-generated name is used)')
    parser.add_argument('--root', dest='root', default=True,
                        action='store_false', help='used to not run this with privilege')
    parser.add_argument('--ldpreload', dest='preload', default=None,
                        type=str, help='Preload library to use, if any')
    args = parser.parse_args()
    assert os.path.exists(
        args.wldir), 'Invalid workload directory {}'.format(args.wldir)
    assert os.path.exists(
        args.testdir), 'Invalid test directory {}'.format(args.testdir)
    workdir = copy_tests(args.wldir, args.testdir)
    if args.log is not None:
        logfile = args.log
    else:
        logfile = './{}_{}_results-{}.log'.format(args.test, args.testdir.replace('/', '-'),
                                                  time.strftime('%Y%m%d-%H%M%S'))
    env = {x: os.environ[x] for x in os.environ}
    if args.preload:
        env['LD_PRELOAD'] = args.preload
    # Now it is time to run filebench.
    test = args.test
    if len(test) < 2 or test[-2:] != '.f':
        test = test + '.f'
    fb_args = []
    if args.root and os.geteuid() != 0:
        fb_args = ['sudo']
    fb_args = fb_args + ['filebench', '-f', test]
    with open(logfile, 'w') as log:
        log.write(' '.join(fb_args) + '\n')
        result = subprocess.run(['mount'], stdout=log, stderr=log)
        if result.returncode == 0:
            result = subprocess.run(fb_args, stdout=log,
                                    stderr=log, cwd=workdir, env=env)
    shutil.rmtree(workdir)
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
