sources = ['testfinesse.c', 'munit.c']
libuuid = cc.find_library('uuid')
python = find_program('python3', 'python')
fptoc = files('fptoc.py')
testfuse_sources = ['testfuse.c', 'munit.c']

r = run_command(python, fptoc)
if r.returncode() != 0
    testfuse_sources += 'pathdata.c'
endif
                
executable('testfinesse', 
            sources,
            dependencies: [deps, librt, libuuid, protoc_c_lib, libfuse], 
            include_directories: include_dirs, 
            link_with : [libfinesse])

executable('testfuse',
            testfuse_sources,
            dependencies: [deps, librt, libuuid, protoc_c_lib],
            include_directories: include_dirs,
            link_with : [libfinesse])
