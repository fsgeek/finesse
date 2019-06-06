sources = ['testnicfs.c', 'munit.c']
libuuid = cc.find_library('uuid')
                 
executable('testnicfs', 
            sources,
            dependencies: [deps, librt, libuuid], 
            include_directories: include_dirs, 
            link_with : [libfinessepl])
