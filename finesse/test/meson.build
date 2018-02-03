sources = ['testfinesse.c', 'munit.c']
libuuid = cc.find_library('uuid')
                 
executable('testfinesse', 
            sources,
            dependencies: [deps, librt, libuuid, protoc_c_lib, libfuse], 
            include_directories: include_dirs, 
            link_with : [libfinesse])
