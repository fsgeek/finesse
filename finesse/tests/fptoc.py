#!/usr/bin/python3
'''
The function of this script is to review the dynamic library path and compute a list of all the things
found there, so we know what to search for when we're testing.
'''
import os
import argparse
import glob
import sys

def get_sys_paths():
    '''This function will look in /etc/ld.so.conf.d/*.conf'''
    searchdir = '/etc/ld.so.conf.d'
    paths = []
    candidates = [x for x in os.listdir(searchdir) if x.endswith('.conf')]
    for candidate in candidates:
        with open('{}/{}'.format(searchdir, candidate), "rt") as fd:
            for line in fd.readlines():
                if line.startswith('#'): continue
                paths.append(line.strip())                
    return paths

def get_search_paths():
    search_paths = []
    if 'LD_LIBRARY_PATH' in os.environ:
        search_paths += os.environ['LD_LIBRARY_PATH'].split(':')
    search_paths += get_sys_paths()
    return search_paths

def generate_sorted_list(files, structname, fd=None):
    print("const char *{}[] = ".format(structname))
    print("{")
    files.sort()
    for f in files:
        print('    "{}",'.format(f))
    print("(void *)0};")    


#generate_sorted_list("all_files_in_path", "files_in_path")
#generate_sorted_list("libs_in_path", "libs_in_path")

def main():
    parser = argparse.ArgumentParser(description='Generate environmental search data for testing.')
    parser.add_argument('--path', dest='path', default=os.environ['PATH'].split(':'), type=list)
    parser.add_argument('--libs', dest='libs', default=get_search_paths(), type=list)
    parser.add_argument('--output', dest = 'output', default='pathdata.c', type=str, nargs=1)
    args = parser.parse_args()

    files = []
    for dir in args.path:
        if not os.path.exists(dir): continue
        cwd = os.getcwd()
        os.chdir(dir)
        files += glob.glob('*')
        os.chdir(cwd)
    print('{} directories to search'.format(len(args.path)))
    print('{} files can be found'.format(len(files)))
    file_dic = {}
    for f in files:
        if f in file_dic: file_dic[f] += 1
        else: file_dic[f] = 1
    print('{} unique names'.format(len(file_dic)))
    print('{} lib dirs to search'.format(len(args.libs)))
    libs = []
    cwd = os.getcwd()
    for dir in args.libs:
        if not os.path.exists(dir): continue
        os.chdir(dir)
        libs += glob.glob('*')
    os.chdir(cwd)
    lib_dic = {}
    for l in libs:
        if l in lib_dic: lib_dic[l] += 1
        else: lib_dic[l] = 1
    print('{} libraries can be found'.format(len(lib_dic)))

    sys.stdout = open(args.output, 'w')
    sys.stderr = sys.stdout

    generate_sorted_list([x for x in file_dic], "files_in_path")
    generate_sorted_list([x for x in lib_dic], 'libs_in_path')

    sys.exit(0)

if __name__ == "__main__":
    main()

