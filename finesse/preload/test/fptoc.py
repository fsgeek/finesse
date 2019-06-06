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

def generate_sorted_list(fname, structname):
    with open(fname) as fd:
        files = [x.strip() for x in fd.readlines()]
    print("const char *{}[] = ".format(structname))
    print("{")
    files.sort();
    for f in files:
        print('"{}",'.format(f))
    print("};")    


generate_sorted_list("all_files_in_path", "files_in_path")
generate_sorted_list("libs_in_path", "libs_in_path")