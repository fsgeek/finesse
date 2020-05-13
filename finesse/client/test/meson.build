libuuid = cc.find_library('uuid')
python = find_program('python3', 'python')
fptoc = files('fptoc.py')
r = run_command(python, fptoc)
assert(r.returncode() == 0, 'Failed to generate pathdata.c correctly')
common_sources = ['munit.c', 'pathdata.c']

testfinesse_sources = common_sources + ['testfinesse.c']
testfuse_sources = common_sources + ['testfuse.c']
                
executable('testfinesse', 
            testfinesse_sources,
            dependencies: [deps, librt, libuuid, protoc_c_lib, libfuse], 
            include_directories: [include_dirs, finesse_inc_dirs], 
            link_with : [libfinesse])

executable('testfuse',
            testfuse_sources,
            dependencies: [deps, librt, libuuid, protoc_c_lib],
            include_directories: [include_dirs, finesse_inc_dirs], 
            link_with : [libfinesse])
