#!/usr/bin/python

#
# TODO: have this script pull the environment stuff and compute this 
#
# To generate this I manully just enumerated:
# static const char *ld_library_path[] = {
# "/usr/lib/x86_64-linux-gnu/libfakeroot",
# "/lib/i386-linux-gnu",
# "/usr/lib/i386-linux-gnu",
# "/usr/lib/i686-linux-gnu",
# "/usr/lib/i386-linux-gnu/mesa",
# "/usr/local/lib",
# "/lib/x86_64-linux-gnu",
# "/usr/lib/x86_64-linux-gnu",
# "/usr/lib/x86_64-linux-gnu/mesa-egl",
# "/usr/lib/x86_64-linux-gnu/mesa",
# "/lib32",
# "/usr/lib32",
# "/libx32",
# "/usr/libx32",
# };

import os
import argparse
import glob
import sys

def generate_sorted_list(files, structname, fd=None):
    print("const char *{}[] = ".format(structname))
    print("{")
    files.sort();
    for f in files:
        print('    "{}",'.format(f))
    print("};")    


#generate_sorted_list("all_files_in_path", "files_in_path")
#generate_sorted_list("libs_in_path", "libs_in_path")

def main():
    parser = argparse.ArgumentParser(description='Generate environmental search data for testing.')
    parser.add_argument('--path', dest='path', default=os.environ['PATH'].split(':'), type=list)
    parser.add_argument('--libs', dest='libs', default=os.environ['LD_LIBRARY_PATH'].split(':'), type=list)
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


    return

if __name__ == "__main__":
    main()

