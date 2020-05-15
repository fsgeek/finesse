
deps = []
deps += cc.find_library('rt')
deps += cc.find_library('uuid')
deps += cc.find_library('libfuse')
deps += cc.find_library('pthread')

libdl = cc.find_library('dl', required: false)
if libdl.found()
   deps += [ libdl ]
endif

finessecommmunications_sources = [
   'fincomm.c', 
   'fcc.c', 
   'fcs.c', 
   'namemap.c', 
   'statfs.c',
   'testmsg.c']

finesscommunications = static_library('finessecommuncations',
                                      finessecommmunications_sources,
                                      dependencies: deps,
                                      c_args: ['-Wall', '-Wextra', '-Werror'],
                                      include_directories: [include_dirs, finesse_inc_dirs])

