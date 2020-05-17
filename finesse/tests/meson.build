libuuid = cc.find_library('uuid')
python = find_program('python3', 'python')
fptoc = files('fptoc.py')
r = run_command(python, fptoc)
assert(r.returncode() == 0, 'Failed to generate pathdata.c correctly')

librt = cc.find_library('rt')
libuuid = cc.find_library('uuid')
libfuse = cc.find_library('libfuse')
libpthread = cc.find_library('pthread')
munit = dependency('munit', fallback: ['munit', 'munit_dep'])

common_sources = [
    'test_base.c',
]

testfinesse_sources = [
    'pathdata.c',
    'finesse_test.c',
    'fincomm_test.c',
]

testutils_sources = [
    'pathdata.c',
    'testutils.c',
]

testfuse_sources = [
    'fuse_test.c',
]



executable('testutils',
           [common_sources, testutils_sources],
           dependencies: [deps, munit],
           include_directories: [include_dirs, finesse_inc_dirs],
           link_with: [finesscommunications, finesse_utils, libfinesse_api]
          )

executable('testfinesse',
           [common_sources, testfinesse_sources],
           dependencies: [deps, munit],
           include_directories: [include_dirs, finesse_inc_dirs],
           link_with: [finesscommunications, finesse_utils, libfinesse_api]
          )

executable('testfuse',
           [common_sources, testfuse_sources],
           dependencies: [deps, munit],
           include_directories: [include_dirs, finesse_inc_dirs],
           link_with: [finesscommunications, finesse_utils, libfinesse_api]
)
