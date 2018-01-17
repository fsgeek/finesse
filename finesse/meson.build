libfinesse_sources = ['finesse.c']

deps = [ thread_dep ]
libdl = cc.find_library('dl', required: false)
if libdl.found()
   deps += [ libdl ]
endif

libfinesse = library('finesse', libfinesse_sources, version: meson.project_version(),
                  soversion: '3', include_directories: include_dirs,
                  dependencies: deps, install: true,
                  c_args: [ '-DFUSE_USE_VERSION=32' ])

