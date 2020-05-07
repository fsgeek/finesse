libuuid = cc.find_library('uuid')
python = find_program('python3', 'python')
test_fincomm_sources = ['munit.c', 'fincomm_test.c']

executable('testfincomm',
           test_fincomm_sources,
           dependencies: [deps],
           include_directories: include_dirs,
           link_with: finesscommunications
          )
