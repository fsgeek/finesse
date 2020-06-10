librt = cc.find_library('rt')
libuuid = cc.find_library('uuid')
libpthread = cc.find_library('pthread')
deps = [librt, libuuid, libpthread]

munit = dependency('munit', fallback: ['munit', 'munit_dep'])

test_bitbucket_sources = [
    'test_bitbucket.c',
    'test_dir.c',
    'test_inode.c',
    'test_object.c',
    'test_trie.c',
]

bitbucket_parent_sources = [
    '../dir.c',
    '../inode.c',
    '../object.c',
    '../trie.c',
    '../xattr.c',
]

executable('testbitbucket',
           [test_bitbucket_sources, bitbucket_parent_sources],
           dependencies: [deps, munit],
           include_directories: [include_dirs, finesse_inc_dirs, '..'],
           link_with: [finesscommunications, finesse_utils, libfinesse_api]
          )
