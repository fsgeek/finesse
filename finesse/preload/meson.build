rpcgen = find_program('rpcgen', required : false)

libfinessepl_sources = [
    'nicfdmgr.c',
    'nicfscomm.c',
    'nicfsinit.c',
    'nicopenclose.c',
    'nicreadwrite.c',
    'nicsearch.c',
    'nicunlink.c',
]

deps = [ thread_dep ]
libdl = cc.find_library('dl', required: false)
if libdl.found()
   deps += [ libdl ]
endif

librt = cc.find_library('rt')
libuuid = cc.find_library('uuid')

libfinessepl = library('finessepl', libfinessepl_sources,
                    version: meson.project_version(),
                    soversion: '1',
                    include_directories: include_dirs,
                    dependencies: deps,
                    install: true,

                )

subdir('test')
