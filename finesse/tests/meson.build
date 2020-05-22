libuuid = cc.find_library('uuid')
python = find_program('python3', 'python')

librt = cc.find_library('rt')
libuuid = cc.find_library('uuid')
libfuse = cc.find_library('libfuse')
libpthread = cc.find_library('pthread')
munit = dependency('munit', fallback: ['munit', 'munit_dep'])

pathdata_c = custom_target(
    'gen-output',
    output: ['pathdata.c'],
    input: ['fptoc.py'],
    command: [python, '@INPUT@', '--output', '@OUTPUT@'])

common_sources = [
    'test_base.c',
]

testfinesse_sources = [
    'finesse_test.c',
    'fincomm_test.c',
]

testutils_sources = [
    'testutils.c',
]

testfuse_sources = [
    'fuse_test.c',
]



executable('testutils',
           [common_sources, testutils_sources, pathdata_c],
           dependencies: [deps, munit],
           include_directories: [include_dirs, finesse_inc_dirs],
           link_with: [finesscommunications, finesse_utils, libfinesse_api]
          )

executable('testfinesse',
           [common_sources, testfinesse_sources, pathdata_c],
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
