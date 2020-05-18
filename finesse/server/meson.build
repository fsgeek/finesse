deps = []
deps += cc.find_library('rt')
deps += cc.find_library('uuid')
deps += cc.find_library('libfuse')
deps += cc.find_library('pthread')

libdl = cc.find_library('dl', required: false)
if libdl.found()
   deps += [ libdl ]
endif

finesse_server_sources = [
   'finesse-req.c',
   'fuse.c',
   'namemap.c',
   'native.c',
   'statfs.c',
   'test.c',
   'util.c',
   ]

finesse_server = static_library('finesse_server',
                                 finesse_server_sources,
                                 dependencies: deps,
                                 c_args: ['-DFUSE_USE_VERSION=39', '-Wall', '-Wextra', '-Werror'],
                                 link_with: finesscommunications,
                                 include_directories: [include_dirs, finesse_inc_dirs])

