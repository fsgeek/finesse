import os
import sys

fuse_lowlevel_h = "include/fuse_lowlevel.h"

with open(fuse_lowlevel_h, "rt") as fd:
    fuse_ll_lines = fd.readlines()

index = 0
for l in fuse_ll_lines:
    # Step (1): find the start of the operations definition
    index = index + 1
    while 'struct fuse_lowlevel_ops' not in l: continue
print('starts on line {}'.format(index))