
libfinesse_api_sources = [
   'access.c',
   'fdmgr.c',
   # 'finesse-search.c',
   'init.c',
   'mkdir.c',
   'openclose.c',
   'stat.c',
   'statfs.c',
   'unlink.c'
]

deps = [ thread_dep ]
libdl = cc.find_library('dl', required: false)
if libdl.found()
   deps += [ libdl ]
endif

librt = cc.find_library('rt')
libuuid = cc.find_library('uuid')
libpthread = cc.find_library('pthread')

libfinesse_api = static_library('finesse_api', libfinesse_api_sources, 
                                 version: meson.project_version(),
                                 soversion: '3', 
                                 include_directories: [include_dirs, finesse_inc_dirs],
                                 dependencies: [deps, libpthread, librt, libuuid], 
                                 install: true,
                                 c_args: [ '-DFUSE_USE_VERSION=39'],
                                 link_with: finesscommunications
)

#subdir('test')
