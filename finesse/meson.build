#
# Read build files from sub-directories
#
subdirs = [ 'include', 'communications', 'utils', 'api', 'server', 'preload', 'tests', 'iowrapper']
foreach n : subdirs
    subdir(n)
endforeach

# note that bitbucket must build after FUSE builds, so it's not included here
