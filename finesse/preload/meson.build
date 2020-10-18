
finesse_preload_sources = [
    'access.c',
    'chdir.c',
    'chmod.c',
    'chown.c',
    'close.c',
    'dir.c',
    'dup.c',
    'init.c',
    'link.c',
    'lseek.c',
    'mkdir.c',
    'open.c',
    'preload.c',
    'read.c',
    'rename.c',
    'rmdir.c',
    'stat.c',
    'unlink.c',
    'utime.c',
    'write.c',
]

librt = cc.find_library('rt')
libuuid = cc.find_library('uuid')
libpthread = cc.find_library('pthread')

deps = [librt, libuuid, libpthread]
libdl = cc.find_library('dl', required: false)
if libdl.found()
   deps += [ libdl ]
endif

libfinessepl = library('finesse_preload', finesse_preload_sources,
                        version: meson.project_version(),
                        soversion: '1',
                        include_directories: include_dirs,
                        dependencies: deps,
                        link_with: [finesscommunications, finesse_utils, libfinesse_api],
                        install: true,
                )
